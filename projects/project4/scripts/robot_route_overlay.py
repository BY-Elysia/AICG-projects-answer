"""Draw a CPU route inset from a loaded scene and an existing verified route.

Arrays returned by ``render_route_overlay`` use RGB order. This module neither
loads files nor plans or modifies routes; the caller owns input validation and
video-frame placement.
"""

from __future__ import annotations

from dataclasses import dataclass
from typing import TYPE_CHECKING

import cv2
import numpy as np

if TYPE_CHECKING:
    from robotnav.navigation.scene.contracts import SceneObstacleModel


@dataclass(frozen=True)
class PreparedRouteOverlay:
    background: np.ndarray
    points_xz: np.ndarray
    bounds_min_xz: np.ndarray
    bounds_max_xz: np.ndarray
    map_rect: tuple[int, int, int, int]
    route_rgb: tuple[int, int, int]
    text_rgb: tuple[int, int, int]
    font_scale: float
    line_width: int
    marker_radius: int
    arrow_size: int
    arrival_tolerance_m: float

    def world_to_pixel(self, points_xz: np.ndarray) -> np.ndarray:
        """Map X right and world Z up into the inset's image coordinates."""
        points = np.asarray(points_xz, dtype=np.float64)
        left, top, width, height = self.map_rect
        normalized = (points - self.bounds_min_xz) / (self.bounds_max_xz - self.bounds_min_xz)
        pixels = np.empty_like(normalized)
        pixels[..., 0] = left + normalized[..., 0] * (width - 1)
        pixels[..., 1] = top + (1.0 - normalized[..., 1]) * (height - 1)
        return np.rint(pixels).astype(np.int32)


def prepare_route_overlay(
    model: SceneObstacleModel,
    points_xz: np.ndarray,
    size: tuple[int, int] = (230, 145),
    padding_m: float = 1.0,
    *,
    header_height: int = 24,
    border_px: int = 4,
    font_scale: float = 0.33,
    line_width: int = 2,
    marker_radius: int = 3,
    arrow_size: int = 7,
    arrival_tolerance_m: float = 0.05,
    route_rgb: tuple[int, int, int] = (0, 174, 239),
    text_rgb: tuple[int, int, int] = (232, 241, 250),
    background_rgb: tuple[int, int, int] = (22, 37, 53),
    unknown_rgb: tuple[int, int, int] = (133, 144, 157),
    free_rgb: tuple[int, int, int] = (222, 238, 225),
    inflated_rgb: tuple[int, int, int] = (196, 148, 143),
    obstacle_rgb: tuple[int, int, int] = (59, 53, 60),
    untraveled_rgb: tuple[int, int, int] = (78, 112, 142),
) -> PreparedRouteOverlay:
    """Build the map once; input ``points_xz`` is the existing smooth route.

    Size is ``(width, height)`` in pixels. World padding expands to preserve the
    metric aspect ratio; cells outside the scene remain unknown gray.
    """
    points = np.asarray(points_xz, dtype=np.float64).copy()
    if points.ndim != 2 or points.shape[1] != 2 or len(points) < 2:
        raise ValueError("points_xz must have shape (T, 2), T >= 2")
    if not np.isfinite(points).all() or padding_m <= 0:
        raise ValueError("Route coordinates must be finite and padding_m positive")
    width, height = map(int, size)
    map_width = width - 2 * border_px
    map_height = height - header_height - 2 * border_px
    if map_width < 20 or map_height < 20:
        raise ValueError("Overlay size is too small for its header and borders")
    lower, upper = points.min(axis=0) - padding_m, points.max(axis=0) + padding_m
    center = (lower + upper) / 2
    span = upper - lower
    aspect = map_width / map_height
    if span[0] / span[1] < aspect:
        span[0] = span[1] * aspect
    else:
        span[1] = span[0] / aspect
    lower, upper = center - span / 2, center + span / 2
    image = np.empty((height, width, 3), dtype=np.uint8)
    image[:] = background_rgb
    map_image = np.empty((map_height, map_width, 3), dtype=np.uint8)
    map_image[:] = unknown_rgb
    world_x = np.linspace(lower[0], upper[0], map_width)
    world_z = np.linspace(upper[1], lower[1], map_height)
    cols = np.floor((world_x - model.origin_xz[0]) / model.resolution_m).astype(int)
    rows = np.floor((world_z - model.origin_xz[1]) / model.resolution_m).astype(int)
    grid_rows, grid_cols = np.meshgrid(rows, cols, indexing="ij")
    scene_height, scene_width = model.planning_blocked.shape
    inside = (
        (grid_rows >= 0) & (grid_rows < scene_height) & (grid_cols >= 0) & (grid_cols < scene_width)
    )
    valid_rows, valid_cols = grid_rows[inside], grid_cols[inside]
    cell_colors = np.empty((inside.sum(), 3), dtype=np.uint8)
    cell_colors[:] = unknown_rgb
    cell_colors[~model.planning_blocked[valid_rows, valid_cols]] = free_rgb
    cell_colors[model.inflated_obstacles[valid_rows, valid_cols]] = inflated_rgb
    cell_colors[model.cleaned_obstacles[valid_rows, valid_cols]] = obstacle_rgb
    map_image[inside] = cell_colors
    left, top = border_px, header_height + border_px
    image[top : top + map_height, left : left + map_width] = map_image
    prepared = PreparedRouteOverlay(
        image,
        points,
        lower,
        upper,
        (left, top, map_width, map_height),
        route_rgb,
        text_rgb,
        font_scale,
        line_width,
        marker_radius,
        arrow_size,
        arrival_tolerance_m,
    )
    cv2.polylines(
        image, [prepared.world_to_pixel(points)], False, untraveled_rgb, line_width, cv2.LINE_AA
    )
    return prepared


def render_route_overlay(
    prepared: PreparedRouteOverlay,
    current_xz: np.ndarray,
    heading: float | np.ndarray,
    progress: float = 0.0,
    arrived: bool = False,
    *,
    current_index: int | None = None,
) -> np.ndarray:
    """Return an RGB inset with the actual position, heading and traveled path.

    ``heading`` is radians ``atan2(tangent_Z, tangent_X)`` or a two-component
    world X-Z direction. ``current_index`` is the original route point at the
    start of the current interpolation segment; passing it avoids ambiguity at
    route crossings. An arrived flag only displays success within tolerance of
    the original final target point.
    """
    current = np.asarray(current_xz, dtype=np.float64)
    if current.shape != (2,) or not np.isfinite(current).all():
        raise ValueError("current_xz must be a finite world X-Z pair")
    if current_index is None:
        current_index = int(np.argmin(np.sum((prepared.points_xz - current) ** 2, axis=1)))
    if not 0 <= current_index < len(prepared.points_xz):
        raise ValueError("current_index must reference an original route point")
    heading_array = np.asarray(heading, dtype=np.float64)
    if heading_array.shape == (2,):
        angle = float(np.arctan2(heading_array[1], heading_array[0]))
    elif heading_array.ndim == 0:
        angle = float(heading_array)
    else:
        raise ValueError("heading must be a radian angle or a world X-Z direction pair")
    if not np.isfinite(angle) or not np.isfinite(progress):
        raise ValueError("heading and progress must be finite")
    remaining_distance = float(np.linalg.norm(current - prepared.points_xz[-1]))
    remaining_route = (1 - np.clip(progress, 0, 1)) * np.linalg.norm(
        np.diff(prepared.points_xz, axis=0), axis=1
    ).sum()
    reached = bool(arrived and remaining_distance <= prepared.arrival_tolerance_m)
    image = prepared.background.copy()
    trail = np.vstack((prepared.points_xz[: current_index + 1], current))
    cv2.polylines(
        image,
        [prepared.world_to_pixel(trail)],
        False,
        prepared.route_rgb,
        prepared.line_width + 1,
        cv2.LINE_AA,
    )
    start_px, goal_px = prepared.world_to_pixel(prepared.points_xz[[0, -1]])
    for label, pixel in (("S", start_px), ("G", goal_px)):
        cv2.circle(image, tuple(pixel), prepared.marker_radius, (245, 252, 255), -1, cv2.LINE_AA)
        cv2.putText(
            image,
            label,
            tuple(pixel + np.array([5, -5])),
            cv2.FONT_HERSHEY_SIMPLEX,
            prepared.font_scale,
            (16, 38, 57),
            1,
            cv2.LINE_AA,
        )
    if reached:
        cv2.circle(image, tuple(goal_px), prepared.arrow_size + 2, (35, 175, 100), 2, cv2.LINE_AA)
    current_px = prepared.world_to_pixel(current)
    forward = np.array([np.cos(angle), -np.sin(angle)])
    side = np.array([-forward[1], forward[0]])
    arrow = np.rint(
        np.array(
            [
                current_px + forward * prepared.arrow_size,
                current_px
                - forward * prepared.arrow_size * 0.65
                + side * prepared.arrow_size * 0.65,
                current_px
                - forward * prepared.arrow_size * 0.65
                - side * prepared.arrow_size * 0.65,
            ]
        )
    ).astype(np.int32)
    cv2.fillConvexPoly(image, arrow, (30, 184, 110) if reached else prepared.route_rgb, cv2.LINE_AA)
    cv2.polylines(image, [arrow], True, (250, 253, 255), 1, cv2.LINE_AA)
    status = "GOAL REACHED" if reached else f"FOLLOW ROUTE {100 * np.clip(progress, 0, 1):.0f}%"
    cv2.putText(
        image,
        status,
        (6, 11),
        cv2.FONT_HERSHEY_SIMPLEX,
        prepared.font_scale,
        prepared.text_rgb,
        1,
        cv2.LINE_AA,
    )
    cv2.putText(
        image,
        f"S = start  G = goal  |  left {remaining_route:.2f} m",
        (6, 22),
        cv2.FONT_HERSHEY_SIMPLEX,
        prepared.font_scale * 0.77,
        prepared.text_rgb,
        1,
        cv2.LINE_AA,
    )
    return image
