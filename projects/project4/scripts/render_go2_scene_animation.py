#!/usr/bin/env python3
"""Animate the real Go2 mesh along a checked route in the original 3DGS scene.

The scene is rendered by the project's gsplat implementation. The robot is
rendered by PyBullet's CPU TinyRenderer, with the same pinhole camera, and is
composited using optical-axis metric depth. Joint motion is a prescribed
kinematic trot: this script does not perform a dynamics or policy rollout.
"""

from __future__ import annotations

import argparse
import json
import math
import subprocess
import time
import xml.etree.ElementTree as ET
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import cv2
import numpy as np
from robot_route_overlay import prepare_route_overlay, render_route_overlay
from robot_scene_geometry import estimate_ground_profile, filter_supported_occlusion

from robotnav.config import (
    _resolve_path,
    load_command_toml,
    load_dataclass_section,
)
from robotnav.dataset.trajectory_manifest import EpisodeSpec, load_trajectory_batch
from robotnav.navigation.scene.artifact import (
    SceneArtifact,
    file_sha256,
    load_scene_artifact,
    source_file_record,
)


@dataclass(frozen=True)
class Paths:
    scene_dir: str
    trajectories_dir: str
    trajectory_manifest_filename: str
    ply_dir: str
    ply_filename: str
    robot_dir: str
    urdf_filename: str
    output_dir: str
    ground_support_path: str


@dataclass(frozen=True)
class Video:
    width: int = 848
    height: int = 480
    fps: int = 20
    duration_s: float = 8.0
    preview_indices: tuple[int, ...] = (0, 80, 159)
    gif_width: int = 636
    gif_fps: int = 10
    annotate: bool = True
    start_hold_s: float = 1.0
    goal_hold_s: float = 2.0
    scene_label: str = "Corridor"


@dataclass(frozen=True)
class Camera:
    distance_m: float = 1.8
    height_m: float = 1.25
    look_ahead_m: float = 0.35
    target_height_m: float = 0.28
    lateral_offset_m: float = 0.0
    fov_y_deg: float = 58.0
    near: float = 0.03
    far: float = 120.0


@dataclass(frozen=True)
class Robot:
    trajectory_id: str = "auto_000"
    base_height_m: float = 0.34
    gait_hz: float = 1.8
    step_length_m: float = 0.18
    foot_lift_m: float = 0.07
    body_bob_m: float = 0.004
    foot_center_height_m: float = 0.022586
    fit_visual_feet_to_ground: bool = True


@dataclass(frozen=True)
class Render:
    max_cuda_memory_fraction: float = 0.2
    sh_degree: str = "auto"
    alpha_threshold: float = 0.2
    occlusion_tolerance_m: float = 0.025
    background_rgb: tuple[float, float, float] = (0.025, 0.03, 0.035)
    geometry_supported_occlusion: bool = True
    geometry_clearance_m: float = 0.16
    geometry_height_max_m: float = 0.65


@dataclass(frozen=True)
class Navigation:
    show_route: bool = True
    route_width_m: float = 0.045
    goal_radius_m: float = 0.26
    inset_width_px: int = 240
    inset_height_px: int = 160
    inset_padding_m: float = 0.8
    margin_px: int = 12
    ground_band_m: float = 0.08
    ground_radius_m: float = 0.25


@dataclass(frozen=True)
class AnimationConfig:
    paths: Paths
    video: Video
    camera: Camera
    robot: Robot
    render: Render
    navigation: Navigation


def load_config(filename: str) -> AnimationConfig:
    raw = load_command_toml(
        filename, sections={"paths", "video", "camera", "robot", "render", "navigation"}
    )
    config = AnimationConfig(
        paths=Paths(**load_dataclass_section(raw, "paths", Paths)),
        video=Video(**load_dataclass_section(raw, "video", Video)),
        camera=Camera(**load_dataclass_section(raw, "camera", Camera)),
        robot=Robot(**load_dataclass_section(raw, "robot", Robot)),
        render=Render(**load_dataclass_section(raw, "render", Render)),
        navigation=Navigation(**load_dataclass_section(raw, "navigation", Navigation)),
    )
    if min(config.video.width, config.video.height, config.video.fps) <= 0:
        raise ValueError("Video width, height, and fps must be positive")
    if config.video.duration_s <= 0 or round(config.video.duration_s * config.video.fps) < 2:
        raise ValueError("Video duration must provide at least two frames")
    if not 0 < config.camera.near < config.camera.far or not 1 < config.camera.fov_y_deg < 175:
        raise ValueError("Camera clipping planes and vertical field of view are invalid")
    if not 0 < config.render.max_cuda_memory_fraction <= 1:
        raise ValueError("CUDA allocator fraction must be in (0, 1]")
    if not 0 <= config.render.alpha_threshold <= 1:
        raise ValueError("Scene alpha threshold must be in [0, 1]")
    if config.robot.base_height_m <= config.robot.foot_center_height_m:
        raise ValueError("Robot base must be above the foot centers")
    if min(config.video.gif_fps, config.video.gif_width) <= 0:
        raise ValueError("GIF width and fps must be positive")
    if (
        min(config.video.start_hold_s, config.video.goal_hold_s) < 0
        or config.video.start_hold_s + config.video.goal_hold_s >= config.video.duration_s
    ):
        raise ValueError("Start and goal holds must leave a positive movement duration")
    if (
        config.navigation.inset_width_px + 2 * config.navigation.margin_px > config.video.width
        or config.navigation.inset_height_px + 2 * config.navigation.margin_px > config.video.height
    ):
        raise ValueError("Route inset must fit inside the video")
    return config


def input_files(config: AnimationConfig) -> dict[str, Path]:
    paths = config.paths
    return {
        "ground_support": _resolve_path(paths.ground_support_path),
        "scene_dir": _resolve_path(paths.scene_dir),
        "trajectory_manifest": (
            _resolve_path(paths.trajectories_dir) / paths.trajectory_manifest_filename
        ),
        "ply": _resolve_path(paths.ply_dir) / paths.ply_filename,
        "urdf": _resolve_path(paths.robot_dir) / paths.urdf_filename,
        "output_dir": _resolve_path(paths.output_dir),
    }


def load_inputs(
    config: AnimationConfig, files: dict[str, Path]
) -> tuple[SceneArtifact, EpisodeSpec, dict[str, Any]]:
    artifact = load_scene_artifact(files["scene_dir"])
    batch = load_trajectory_batch(files["trajectory_manifest"], files["output_dir"] / "unused")
    if batch.source_scene_model_sha256 != artifact.model_sha256:
        raise ValueError("Configured route manifest belongs to a different scene artifact")
    episodes = [item for item in batch.episodes if item.trajectory_id == config.robot.trajectory_id]
    if len(episodes) != 1:
        raise ValueError(f"No unique trajectory {config.robot.trajectory_id!r} in the manifest")
    episode = episodes[0]
    if not np.isclose(episode.floor_y, artifact.model.floor_y, atol=1e-9):
        raise ValueError("Route floor height differs from the integrity-checked scene")
    if not files["urdf"].is_file():
        raise FileNotFoundError(files["urdf"])
    if not files["ply"].is_file():
        raise FileNotFoundError(files["ply"])
    source = {
        "ground_support": source_file_record(files["ground_support"]),
        "scene_model_sha256": artifact.model_sha256,
        "las_source_record_from_scene_manifest": artifact.source_las,
        "trajectory_manifest": source_file_record(files["trajectory_manifest"]),
        "trajectory_id": episode.trajectory_id,
        "trajectory": source_file_record(episode.trajectory_path),
        "ply": source_file_record(files["ply"]),
        "robot_urdf": source_file_record(files["urdf"]),
    }
    asset_manifest = files["urdf"].parent / "asset_manifest.json"
    if asset_manifest.is_file():
        source["robot_asset_manifest"] = json.loads(asset_manifest.read_text(encoding="utf-8"))
    return artifact, episode, source


def resample_route(
    points: np.ndarray, count: int, progress: np.ndarray | None = None
) -> tuple[np.ndarray, np.ndarray, float]:
    """Interpolate position by arc length and derive smooth planar heading."""
    lengths = np.linalg.norm(np.diff(points, axis=0), axis=1)
    cumulative = np.r_[0.0, np.cumsum(lengths)]
    keep = np.r_[True, np.diff(cumulative) > 1e-10]
    points = points[keep]
    cumulative = cumulative[keep]
    if len(points) < 2 or cumulative[-1] <= 0:
        raise ValueError("Route must contain at least two distinct coordinates")
    if progress is None:
        progress = np.linspace(0, 1, count)
    if progress.shape != (count,) or np.any((progress < 0) | (progress > 1)):
        raise ValueError("Route progress must provide one fraction in [0,1] per frame")
    distance = progress * cumulative[-1]
    positions = np.stack(
        [np.interp(distance, cumulative, points[:, axis]) for axis in range(2)], axis=1
    )
    look_span = min(0.12, cumulative[-1] / 8)
    ahead = np.minimum(distance + look_span, cumulative[-1])
    behind = np.maximum(distance - look_span, 0)
    tangents = np.stack(
        [
            np.interp(ahead, cumulative, points[:, axis])
            - np.interp(behind, cumulative, points[:, axis])
            for axis in range(2)
        ],
        axis=1,
    )
    headings = np.unwrap(np.arctan2(tangents[:, 1], tangents[:, 0]))
    return positions, headings, float(cumulative[-1])


def scene_to_zup_matrix(floor_y: float) -> np.ndarray:
    """Map original [X,Y,Z] to display [X,Z,floor_y-Y], without scene scaling."""
    return np.array(
        [[1, 0, 0, 0], [0, 0, 1, 0], [0, -1, 0, floor_y], [0, 0, 0, 1]],
        dtype=np.float64,
    )


def metric_depth_from_zbuffer(zbuffer: np.ndarray, near: float, far: float) -> np.ndarray:
    return (far * near / (far - (far - near) * zbuffer)).astype(np.float32)


class Go2Renderer:
    """Render the original mesh with a prescribed analytical leg trajectory."""

    def __init__(self, urdf: Path, config: AnimationConfig):
        import pybullet as bullet

        self.bullet = bullet
        self.config = config
        self.client = bullet.connect(bullet.DIRECT)
        self.body = bullet.loadURDF(
            str(urdf),
            useFixedBase=True,
            flags=bullet.URDF_IGNORE_COLLISION_SHAPES,
            physicsClientId=self.client,
        )
        self.joints: dict[str, int] = {}
        self.links: dict[str, int] = {}
        self.limits: dict[str, tuple[float, float]] = {}
        for index in range(bullet.getNumJoints(self.body, physicsClientId=self.client)):
            info = bullet.getJointInfo(self.body, index, physicsClientId=self.client)
            name = info[1].decode()
            self.joints[name] = index
            self.links[info[12].decode()] = index
            self.limits[name] = (float(info[8]), float(info[9]))
        root = ET.parse(urdf).getroot()
        self.leg_lengths: dict[str, tuple[float, float]] = {}
        self.foot_vertices: dict[str, np.ndarray] = {}
        mesh_cache: dict[Path, np.ndarray] = {}
        for leg in ("FL", "FR", "RL", "RR"):
            upper = root.find(f"./joint[@name='{leg}_calf_joint']/origin")
            lower = root.find(f"./joint[@name='{leg}_foot_joint']/origin")
            if upper is None or lower is None or f"{leg}_foot" not in self.links:
                raise ValueError(f"URDF lacks the expected real Go2 {leg} leg chain")
            a = np.fromstring(upper.attrib["xyz"], sep=" ")
            b = np.fromstring(lower.attrib["xyz"], sep=" ")
            if not np.allclose(a[:2], 0) or not np.allclose(b[:2], 0):
                raise ValueError("Analytical trot requires Go2's unrotated vertical leg links")
            self.leg_lengths[leg] = (float(-a[2]), float(-b[2]))
            parts = []
            for visual in root.findall(f"./link[@name='{leg}_foot']/visual"):
                mesh = visual.find("geometry/mesh")
                if mesh is None:
                    raise ValueError("Real Go2 foot visuals must use the converted original mesh")
                mesh_path = urdf.parent / mesh.attrib["filename"]
                if mesh_path not in mesh_cache:
                    with mesh_path.open(encoding="utf-8") as handle:
                        vertices = [
                            np.fromstring(line[2:], sep=" ")
                            for line in handle
                            if line.startswith("v ")
                        ]
                    mesh_cache[mesh_path] = np.asarray(vertices, dtype=np.float64)
                vertices = mesh_cache[mesh_path]
                scale = np.fromstring(mesh.attrib.get("scale", "1 1 1"), sep=" ")
                origin = visual.find("origin")
                origin_values = {} if origin is None else origin.attrib
                xyz = np.fromstring(origin_values.get("xyz", "0 0 0"), sep=" ")
                rpy = np.fromstring(origin_values.get("rpy", "0 0 0"), sep=" ")
                rotation = np.asarray(
                    bullet.getMatrixFromQuaternion(bullet.getQuaternionFromEuler(rpy))
                ).reshape(3, 3)
                parts.append((vertices * scale) @ rotation.T + xyz)
            if not parts:
                raise ValueError("Real Go2 foot visuals are missing")
            self.foot_vertices[leg] = np.concatenate(parts)
        dynamics = bullet.getDynamicsInfo(self.body, -1, physicsClientId=self.client)
        self.inertial_position, self.inertial_orientation = dynamics[3:5]
        self.projection = bullet.computeProjectionMatrixFOV(
            fov=config.camera.fov_y_deg,
            aspect=config.video.width / config.video.height,
            nearVal=config.camera.near,
            farVal=config.camera.far,
        )
        self.marker_bodies: list[int] = []
        self.marker_mask = np.zeros((config.video.height, config.video.width), dtype=bool)

    def add_route_markers(self, points_xz: np.ndarray, ground_heights: np.ndarray) -> None:
        """Add visual annotations at the actual planned coordinates, without collisions."""
        if not self.config.navigation.show_route:
            return
        bullet = self.bullet
        directions = np.gradient(points_xz, axis=0)
        directions /= np.maximum(np.linalg.norm(directions, axis=1, keepdims=True), 1e-12)
        normals = np.stack([-directions[:, 1], directions[:, 0]], axis=1)
        width = self.config.navigation.route_width_m / 2
        vertices = []
        for point, normal, height in zip(points_xz, normals, ground_heights, strict=True):
            for sign in (-1, 1):
                vertices.append([*(point + sign * width * normal), float(height) + 0.012])
        indices = []
        for index in range(len(points_xz) - 1):
            offset = 2 * index
            indices.extend([offset, offset + 2, offset + 1, offset + 1, offset + 2, offset + 3])
        visual = bullet.createVisualShape(
            bullet.GEOM_MESH,
            vertices=vertices,
            indices=indices,
            rgbaColor=[0.05, 0.85, 1.0, 1.0],
            physicsClientId=self.client,
        )
        self.marker_bodies.append(
            bullet.createMultiBody(
                baseMass=0, baseVisualShapeIndex=visual, physicsClientId=self.client
            )
        )
        for index, radius, color in (
            (0, 0.13, [0.10, 0.90, 0.40, 1]),
            (-1, self.config.navigation.goal_radius_m, [1.0, 0.70, 0.05, 1]),
        ):
            visual = bullet.createVisualShape(
                bullet.GEOM_CYLINDER,
                radius=radius,
                length=0.012,
                rgbaColor=color,
                physicsClientId=self.client,
            )
            self.marker_bodies.append(
                bullet.createMultiBody(
                    baseMass=0,
                    baseVisualShapeIndex=visual,
                    basePosition=[*points_xz[index], float(ground_heights[index]) + 0.015],
                    physicsClientId=self.client,
                )
            )

    def close(self) -> None:
        self.bullet.disconnect(self.client)

    def pose(
        self,
        position_xz: np.ndarray,
        heading: float,
        time_s: float,
        ground_height_m: float = 0.0,
        motion_strength: float = 1.0,
    ) -> dict[str, Any]:
        robot = self.config.robot
        bullet = self.bullet
        phase = float(time_s) * robot.gait_hz
        base_height = (
            ground_height_m
            + robot.base_height_m
            + motion_strength * robot.body_bob_m * math.cos(4 * math.pi * phase)
        )
        base_origin = [float(position_xz[0]), float(position_xz[1]), base_height]
        quaternion = bullet.getQuaternionFromEuler([0, 0, heading])
        # PyBullet resets the center of mass, while a route describes the URDF base link.
        com_position, com_orientation = bullet.multiplyTransforms(
            base_origin, quaternion, self.inertial_position, self.inertial_orientation
        )
        bullet.resetBasePositionAndOrientation(
            self.body, com_position, com_orientation, physicsClientId=self.client
        )
        foot_targets: dict[str, float] = {}
        foot_stance: dict[str, bool] = {}
        for leg in ("FL", "FR", "RL", "RR"):
            local_phase = (phase + (0.5 if leg in ("FR", "RL") else 0)) % 1.0
            half_stride = motion_strength * robot.step_length_m / 2
            stance = local_phase < 0.5 or motion_strength == 0
            if stance:
                fore_aft = half_stride * (1 - 4 * local_phase)
                lift = 0.0
            else:
                swing_phase = 2 * local_phase - 1
                fore_aft = -half_stride * math.cos(math.pi * swing_phase)
                lift = motion_strength * robot.foot_lift_m * math.sin(math.pi * swing_phase)
            target_height = ground_height_m + robot.foot_center_height_m + lift
            upper, lower = self.leg_lengths[leg]
            x = fore_aft
            for iteration in range(5):
                down = base_height - target_height
                cosine = (x * x + down * down - upper * upper - lower * lower) / (2 * upper * lower)
                if not -1 <= cosine <= 1:
                    raise ValueError(
                        f"Unreachable {leg} foot target; adjust robot height/stride/lift"
                    )
                calf = -math.acos(cosine)
                thigh = math.atan2(-x, down) - math.atan2(
                    lower * math.sin(calf), upper + lower * math.cos(calf)
                )
                if robot.fit_visual_feet_to_ground and iteration < 4:
                    pitch = thigh + calf
                    vertices = self.foot_vertices[leg]
                    visual_min_z = np.min(
                        -math.sin(pitch) * vertices[:, 0] + math.cos(pitch) * vertices[:, 2]
                    )
                    target_height = ground_height_m + lift - float(visual_min_z)
            for name, angle in (
                (f"{leg}_hip_joint", 0.0),
                (f"{leg}_thigh_joint", thigh),
                (f"{leg}_calf_joint", calf),
            ):
                lo, hi = self.limits[name]
                if not lo <= angle <= hi:
                    raise ValueError(f"Trot target exceeds the original URDF joint limit: {name}")
                bullet.resetJointState(
                    self.body, self.joints[name], angle, physicsClientId=self.client
                )
            foot_targets[leg] = target_height
            foot_stance[leg] = stance
        actual: dict[str, float] = {}
        visual_minima: dict[str, float] = {}
        for leg in ("FL", "FR", "RL", "RR"):
            state = bullet.getLinkState(
                self.body,
                self.links[f"{leg}_foot"],
                computeForwardKinematics=True,
                physicsClientId=self.client,
            )
            actual[leg] = float(state[4][2])
            rotation = np.asarray(bullet.getMatrixFromQuaternion(state[5])).reshape(3, 3)
            visual_minima[leg] = float(np.min(self.foot_vertices[leg] @ rotation[2]) + actual[leg])
        return {
            "base_origin_zup_m": base_origin,
            "yaw_rad": float(heading),
            "ground_height_m": ground_height_m,
            "motion_strength": motion_strength,
            "foot_target_center_heights_m": foot_targets,
            "foot_actual_center_heights_m": actual,
            "foot_visual_min_heights_m": visual_minima,
            "foot_stance": foot_stance,
            "foot_center_max_error_m": max(abs(actual[leg] - foot_targets[leg]) for leg in actual),
        }

    def camera_matrices(
        self,
        position_xz: np.ndarray,
        heading: float,
        floor_y: float,
        ground_height_m: float = 0.0,
    ) -> tuple[list[float], np.ndarray, dict[str, Any]]:
        camera = self.config.camera
        forward = np.array([math.cos(heading), math.sin(heading), 0.0])
        left = np.array([-math.sin(heading), math.cos(heading), 0.0])
        center = np.array([*position_xz, 0.0])
        eye = center - camera.distance_m * forward + camera.lateral_offset_m * left
        eye[2] = ground_height_m + camera.height_m
        target = center + camera.look_ahead_m * forward
        target[2] = ground_height_m + camera.target_height_m
        view = self.bullet.computeViewMatrix(eye.tolist(), target.tolist(), [0, 0, 1])
        gl_from_zup = np.asarray(view).reshape(4, 4, order="F")
        optical_from_gl = np.diag([1.0, -1.0, -1.0, 1.0])
        optical_from_scene = optical_from_gl @ gl_from_zup @ scene_to_zup_matrix(floor_y)
        return (
            view,
            optical_from_scene.astype(np.float32),
            {
                "eye_zup_m": eye.tolist(),
                "target_zup_m": target.tolist(),
                "optical_camera_from_scene_row_major": optical_from_scene.reshape(-1).tolist(),
            },
        )

    def render(self, view: list[float]) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
        video, camera = self.config.video, self.config.camera
        result = self.bullet.getCameraImage(
            video.width,
            video.height,
            viewMatrix=view,
            projectionMatrix=self.projection,
            renderer=self.bullet.ER_TINY_RENDERER,
            flags=self.bullet.ER_SEGMENTATION_MASK_OBJECT_AND_LINKINDEX,
            shadow=1,
            lightDirection=[-2, -3, 6],
            lightColor=[1, 1, 1],
            lightAmbientCoeff=0.55,
            lightDiffuseCoeff=0.65,
            lightSpecularCoeff=0.08,
            physicsClientId=self.client,
        )
        rgb = np.asarray(result[2], dtype=np.uint8).reshape(video.height, video.width, 4)[..., :3]
        depth = metric_depth_from_zbuffer(
            np.asarray(result[3]).reshape(video.height, video.width), camera.near, camera.far
        )
        segmentation = np.asarray(result[4], dtype=np.int32).reshape(video.height, video.width)
        mask = (segmentation >= 0) & ((segmentation & 0xFFFFFF) == self.body)
        self.marker_mask = (segmentation >= 0) & np.isin(
            segmentation & 0xFFFFFF, self.marker_bodies
        )
        return rgb, depth, mask


class SceneRenderer:
    def __init__(self, ply: Path, config: AnimationConfig):
        import torch

        from robotnav.gpu_environment import require_cuda_environment

        require_cuda_environment()
        torch.cuda.set_per_process_memory_fraction(config.render.max_cuda_memory_fraction)
        torch.cuda.reset_peak_memory_stats()
        from robotnav.rendering.render_one_view import _rasterize_compat, load_ply_to_torch

        self.torch = torch
        self.rasterize = _rasterize_compat
        self.config = config
        self.device = torch.device("cuda")
        loaded = load_ply_to_torch(
            ply,
            device=self.device,
            unit_scale=1.0,
            max_gaussians=0,
            sh_degree_arg=config.render.sh_degree,
        )
        self.means, self.scales, self.quats, self.opacities, self.colors = loaded[:5]
        self.sh_degree = loaded[-1]
        focal = config.video.height / (2 * math.tan(math.radians(config.camera.fov_y_deg) / 2))
        self.intrinsics = torch.tensor(
            [[focal, 0, config.video.width / 2], [0, focal, config.video.height / 2], [0, 0, 1]],
            dtype=torch.float32,
            device=self.device,
        )
        self.background = torch.tensor(
            [config.render.background_rgb], dtype=torch.float32, device=self.device
        )

    def render(self, view: np.ndarray) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
        torch = self.torch
        config = self.config
        with torch.inference_mode():
            renders, alphas, _meta = self.rasterize(
                means=self.means,
                scales=self.scales,
                quats=self.quats,
                opacities=self.opacities,
                colors=self.colors,
                viewmats=torch.as_tensor(view, device=self.device).unsqueeze(0),
                Ks=self.intrinsics.unsqueeze(0),
                width=config.video.width,
                height=config.video.height,
                render_mode="RGB+ED",
                sh_degree=self.sh_degree,
                near_plane=config.camera.near,
                far_plane=config.camera.far,
                packed=True,
            )
            # RGB+ED background handling differs across gsplat API revisions.
            # Render on black, then add the RGB background using the returned alpha.
            rgb_tensor = renders[0, ..., :3] + (1 - alphas[0]) * self.background
            rgb = (rgb_tensor.clamp(0, 1) * 255).round().to(torch.uint8).cpu().numpy()
            depth = renders[0, ..., 3].cpu().numpy().astype(np.float32)
            alpha = alphas[0, ..., 0].cpu().numpy().astype(np.float32)
        return rgb, depth, alpha

    def metrics(self) -> dict[str, Any]:
        torch = self.torch
        return {
            "gaussian_count": int(self.means.shape[0]),
            "sampled": False,
            "unit_scale": 1.0,
            "sh_degree": self.sh_degree,
            "gpu": torch.cuda.get_device_name(0),
            "peak_memory_allocated_bytes": torch.cuda.max_memory_allocated(),
            "peak_memory_reserved_bytes": torch.cuda.max_memory_reserved(),
            "intrinsics": self.intrinsics.cpu().numpy().tolist(),
            "max_cuda_memory_fraction": self.config.render.max_cuda_memory_fraction,
        }


def compose_depth(
    scene_rgb: np.ndarray,
    scene_depth: np.ndarray,
    scene_alpha: np.ndarray,
    robot_rgb: np.ndarray,
    robot_depth: np.ndarray,
    robot_mask: np.ndarray,
    render: Render,
) -> tuple[np.ndarray, np.ndarray]:
    scene_valid = (
        (scene_alpha >= render.alpha_threshold) & np.isfinite(scene_depth) & (scene_depth > 0)
    )
    visible = robot_mask & (
        ~scene_valid | (robot_depth <= scene_depth + render.occlusion_tolerance_m)
    )
    composite = scene_rgb.copy()
    composite[visible] = robot_rgb[visible]
    return composite, visible


def annotate_frame(
    rgb: np.ndarray,
    frame: int,
    config: AnimationConfig,
    length: float,
    progress: float,
    arrived: bool,
) -> None:
    if not config.video.annotate:
        return
    cv2.rectangle(rgb, (12, 12), (min(config.video.width - 12, 670), 72), (12, 16, 22), -1)
    cv2.putText(
        rgb,
        f"{config.video.scene_label} / Go2 - planned route",
        (24, 37),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.62,
        (245, 245, 245),
        1,
        cv2.LINE_AA,
    )
    cv2.putText(
        rgb,
        f"Travel {progress * length:.2f}/{length:.2f} m | "
        f"Remain {(1 - progress) * length:.2f} m | {frame / config.video.fps:.2f} s",
        (24, 59),
        cv2.FONT_HERSHEY_SIMPLEX,
        0.48,
        (190, 205, 215),
        1,
        cv2.LINE_AA,
    )
    if arrived:
        cv2.rectangle(
            rgb, (12, config.video.height - 60), (330, config.video.height - 12), (13, 88, 54), -1
        )
        cv2.putText(
            rgb,
            "GOAL REACHED - stopped",
            (24, config.video.height - 29),
            cv2.FONT_HERSHEY_SIMPLEX,
            0.62,
            (240, 255, 243),
            1,
            cv2.LINE_AA,
        )


def label_goal(
    rgb: np.ndarray, point_xz: np.ndarray, height: float, view: np.ndarray, config: AnimationConfig
) -> None:
    """Project a target annotation from the actual planned endpoint."""
    world = np.array([point_xz[0], height, point_xz[1], 1.0])
    optical = view @ world
    if optical[2] <= config.camera.near:
        return
    focal = config.video.height / (2 * math.tan(math.radians(config.camera.fov_y_deg) / 2))
    uv = (
        np.array([config.video.width / 2, config.video.height / 2])
        + focal * optical[:2] / optical[2]
    )
    x, y = np.rint(uv).astype(int)
    if 12 < x < config.video.width - 90 and 85 < y < config.video.height - 20:
        for color, thickness in (((15, 20, 25), 3), ((255, 207, 53), 1)):
            cv2.putText(
                rgb, "TARGET", (x, y), cv2.FONT_HERSHEY_SIMPLEX, 0.45, color, thickness, cv2.LINE_AA
            )


def save_rgb(path: Path, rgb: np.ndarray) -> None:
    if not cv2.imwrite(str(path), cv2.cvtColor(rgb, cv2.COLOR_RGB2BGR)):
        raise RuntimeError(f"Could not write {path}")


class VideoWriter:
    """Stream RGB frames to the installed ffmpeg, without a Python codec package."""

    def __init__(self, path: Path, video: Video):
        self.process = subprocess.Popen(
            [
                "ffmpeg",
                "-hide_banner",
                "-loglevel",
                "error",
                "-y",
                "-f",
                "rawvideo",
                "-vcodec",
                "rawvideo",
                "-pix_fmt",
                "rgb24",
                "-s",
                f"{video.width}x{video.height}",
                "-r",
                str(video.fps),
                "-i",
                "pipe:0",
                "-an",
                "-c:v",
                "libx264",
                "-preset",
                "medium",
                "-crf",
                "18",
                "-pix_fmt",
                "yuv420p",
                "-movflags",
                "+faststart",
                str(path),
            ],
            stdin=subprocess.PIPE,
        )

    def append_data(self, rgb: np.ndarray) -> None:
        if self.process.stdin is None:
            raise RuntimeError("ffmpeg has no input pipe")
        self.process.stdin.write(np.ascontiguousarray(rgb).tobytes())

    def close(self) -> None:
        if self.process.stdin is not None:
            self.process.stdin.close()
        status = self.process.wait(timeout=60)
        if status:
            raise RuntimeError(f"ffmpeg video encoding failed with exit code {status}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--config", required=True)
    parser.add_argument(
        "--preview-only", action="store_true", help="Render only configured QA frames"
    )
    parser.add_argument(
        "--robot-only", action="store_true", help="CPU mesh/kinematics QA; skips gsplat"
    )
    args = parser.parse_args()
    if args.robot_only and not args.preview_only:
        parser.error("--robot-only requires --preview-only")
    config = load_config(args.config)
    files = input_files(config)
    started = time.perf_counter()
    artifact, episode, sources = load_inputs(config, files)
    output_dir = files["output_dir"]
    output_dir.mkdir(parents=True, exist_ok=True)
    frame_count = round(config.video.duration_s * config.video.fps)
    times = np.arange(frame_count) / config.video.fps
    movement_duration = (
        config.video.duration_s - config.video.start_hold_s - config.video.goal_hold_s
    )
    movement_fraction = np.clip((times - config.video.start_hold_s) / movement_duration, 0, 1)
    progress = movement_fraction**2 * (3 - 2 * movement_fraction)
    positions, headings, route_length = resample_route(episode.points_xz, frame_count, progress)
    ground_heights, ground_report = estimate_ground_profile(
        files["ground_support"],
        np.vstack((positions, episode.points_xz)),
        artifact.model.floor_y,
        artifact.model.axis_transform,
        band_m=config.navigation.ground_band_m,
        radius_m=config.navigation.ground_radius_m,
    )
    route_ground_heights = ground_heights[frame_count:]
    ground_heights = ground_heights[:frame_count]
    route_distance = np.r_[
        0.0, np.cumsum(np.linalg.norm(np.diff(episode.points_xz, axis=0), axis=1))
    ]
    overlay = prepare_route_overlay(
        artifact.model,
        episode.points_xz,
        size=(config.navigation.inset_width_px, config.navigation.inset_height_px),
        padding_m=config.navigation.inset_padding_m,
    )
    preview_indices = sorted({int(index) for index in config.video.preview_indices})
    if any(index < 0 or index >= frame_count for index in preview_indices):
        raise ValueError("Preview frame index is outside the configured video")
    frame_indices = preview_indices if args.preview_only else list(range(frame_count))
    print(f"Animation: {len(frame_indices)} frames, route={route_length:.3f} m", flush=True)
    scene = None if args.robot_only else SceneRenderer(files["ply"], config)
    robot = Go2Renderer(files["urdf"], config)
    robot.add_route_markers(episode.points_xz, route_ground_heights)
    writer = None
    gif_frames: list[np.ndarray] = []
    samples: list[dict[str, Any]] = []
    previews: list[np.ndarray] = []
    gif_stride = max(1, round(config.video.fps / config.video.gif_fps))
    mp4_path = output_dir / "go2_real_scene.mp4"
    try:
        if not args.preview_only:
            writer = VideoWriter(mp4_path, config.video)
        for index in frame_indices:
            frame_started = time.perf_counter()
            arrived = bool(
                progress[index] >= 1
                and np.linalg.norm(positions[index] - episode.points_xz[-1]) <= 1e-6
            )
            strength = float(
                min(1.0, movement_fraction[index] / 0.08, (1 - movement_fraction[index]) / 0.08)
            )
            pose = robot.pose(
                positions[index],
                float(headings[index]),
                max(0.0, times[index] - config.video.start_hold_s),
                float(ground_heights[index]),
                strength,
            )
            view, scene_view, camera_info = robot.camera_matrices(
                positions[index],
                float(headings[index]),
                artifact.model.floor_y,
                float(ground_heights[index]),
            )
            robot_rgb, robot_depth, robot_mask = robot.render(view)
            render_mask = robot_mask | robot.marker_mask
            occlusion_stats = {}
            if scene is None:
                scene_rgb = np.full_like(robot_rgb, 25)
                composite = scene_rgb.copy()
                composite[render_mask] = robot_rgb[render_mask]
                visible = render_mask
            else:
                scene_rgb, scene_depth, scene_alpha = scene.render(scene_view)
                if config.render.geometry_supported_occlusion:
                    visible, occlusion_stats = filter_supported_occlusion(
                        scene_depth,
                        scene_alpha,
                        robot_depth,
                        render_mask,
                        scene_view,
                        scene.intrinsics.cpu().numpy(),
                        artifact.model,
                        alpha_threshold=config.render.alpha_threshold,
                        depth_tolerance=config.render.occlusion_tolerance_m,
                        height_min_m=-config.navigation.ground_band_m,
                        height_max_m=config.render.geometry_height_max_m,
                        clearance_m=config.render.geometry_clearance_m,
                    )
                    composite = scene_rgb.copy()
                    composite[visible] = robot_rgb[visible]
                else:
                    composite, visible = compose_depth(
                        scene_rgb,
                        scene_depth,
                        scene_alpha,
                        robot_rgb,
                        robot_depth,
                        render_mask,
                        config.render,
                    )
            robot_visible = visible & robot_mask
            label_goal(
                composite,
                episode.points_xz[-1],
                artifact.model.floor_y - route_ground_heights[-1] - 0.55,
                scene_view,
                config,
            )
            annotate_frame(composite, index, config, route_length, float(progress[index]), arrived)
            segment_index = int(
                np.clip(
                    np.searchsorted(route_distance, progress[index] * route_length, side="right")
                    - 1,
                    0,
                    len(episode.points_xz) - 1,
                )
            )
            inset = render_route_overlay(
                overlay,
                positions[index],
                headings[index],
                float(progress[index]),
                arrived,
                current_index=segment_index,
            )
            nav = config.navigation
            composite[
                -nav.margin_px - nav.inset_height_px : -nav.margin_px,
                -nav.margin_px - nav.inset_width_px : -nav.margin_px,
            ] = inset
            if index in preview_indices:
                suffix = "robot_only" if args.robot_only else "composite"
                save_rgb(output_dir / f"frame_{index:04d}_{suffix}.png", composite)
                if scene is not None:
                    save_rgb(output_dir / f"frame_{index:04d}_scene.png", scene_rgb)
                    mask_image = np.zeros_like(robot_rgb)
                    mask_image[robot_mask] = [200, 60, 60]
                    mask_image[robot_visible] = [70, 220, 130]
                    save_rgb(output_dir / f"frame_{index:04d}_occlusion.png", mask_image)
                previews.append(composite)
            if writer is not None:
                writer.append_data(composite)
                if index % gif_stride == 0:
                    gif_height = round(
                        config.video.height * config.video.gif_width / config.video.width
                    )
                    gif_frames.append(cv2.resize(composite, (config.video.gif_width, gif_height)))
            samples.append(
                {
                    "frame_index": index,
                    "time_s": index / config.video.fps,
                    "pose": pose,
                    "route_progress": float(progress[index]),
                    "remaining_route_m": float((1 - progress[index]) * route_length),
                    "goal_distance_m": float(
                        np.linalg.norm(positions[index] - episode.points_xz[-1])
                    ),
                    "arrived": arrived,
                    "occlusion": occlusion_stats,
                    "camera": camera_info,
                    "robot_pixels": int(robot_mask.sum()),
                    "visible_robot_pixels": int(robot_visible.sum()),
                    "render_seconds": time.perf_counter() - frame_started,
                }
            )
            if index in preview_indices or index % 20 == 0:
                print(
                    f"Frame {index}/{frame_count - 1}: {samples[-1]['render_seconds']:.3f} s, "
                    f"robot visible {robot_visible.sum()}/{robot_mask.sum()} pixels, "
                    f"progress={progress[index]:.1%}, arrived={arrived}",
                    flush=True,
                )
    finally:
        if writer is not None:
            writer.close()
        robot.close()
    outputs = {}
    if previews:
        contact = np.concatenate(previews, axis=1)
        contact_path = output_dir / (
            "robot_only_preview.png" if args.robot_only else "preview_contact_sheet.png"
        )
        save_rgb(contact_path, contact)
        outputs["preview_contact_sheet"] = str(contact_path)
    if gif_frames:
        import imageio.v2 as imageio

        gif_path = output_dir / "go2_real_scene.gif"
        imageio.mimsave(
            str(gif_path),
            gif_frames,
            duration=1000 * gif_stride / config.video.fps,
            loop=0,
        )
        outputs["gif"] = source_file_record(gif_path)
        outputs["mp4"] = source_file_record(mp4_path)
    report = {
        "purpose": "Real reconstructed scene and original Unitree Go2 mesh animation",
        "kinematic_not_physics": True,
        "motion": "Analytical leg IK, diagonal trot, route arc-length interpolation",
        "scene_geometry": "Lossless frustum crop of original filtered 3DGS; all SH properties retained, unit_scale=1",
        "scene_to_zup": "[worldX, worldZ, floor_y-worldY]",
        "floor_y": artifact.model.floor_y,
        "route_length_m": route_length,
        "route_source_points": len(episode.points_xz),
        "frame_count": len(samples),
        "configured_frame_count": frame_count,
        "fps": config.video.fps,
        "duration_s": config.video.duration_s,
        "scene_label": config.video.scene_label,
        "start_hold_s": config.video.start_hold_s,
        "goal_hold_s": config.video.goal_hold_s,
        "final_goal_distance_m": samples[-1]["goal_distance_m"],
        "final_arrived": samples[-1]["arrived"],
        "ground_profile": ground_report,
        "resolution": [config.video.width, config.video.height],
        "preview_only": args.preview_only,
        "robot_only": args.robot_only,
        "sources": sources,
        "config_file_sha256": file_sha256(_resolve_path("configs") / args.config),
        "renderer": None if scene is None else scene.metrics(),
        "compositing": {
            "depth": "PyBullet optical metric Z versus gsplat RGB+ED expected optical Z",
            "alpha_threshold": config.render.alpha_threshold,
            "occlusion_tolerance_m": config.render.occlusion_tolerance_m,
            "geometry_supported_occlusion": config.render.geometry_supported_occlusion,
            "note": "LAS-ground-supported free-space rejects floating Gaussian depth; actual obstacles, unknown space and high occluders retain depth occlusion. Geometry is approximate, and no scene physics was simulated.",
        },
        "foot_center_max_error_m": max(item["pose"]["foot_center_max_error_m"] for item in samples),
        "elapsed_seconds": time.perf_counter() - started,
        "outputs": outputs,
        "frames": samples,
    }
    report_path = output_dir / (
        "robot_only_report.json"
        if args.robot_only
        else ("preview_report.json" if args.preview_only else "animation_report.json")
    )
    report_path.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"Animation report: {report_path}", flush=True)


if __name__ == "__main__":
    main()
