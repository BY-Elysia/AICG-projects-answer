"""Portable orchestration; all data paths and stage parameters come from TOML."""

from __future__ import annotations

import argparse
import json
import shutil
import subprocess
import sys
from pathlib import Path

import numpy as np

from robotnav.config import PROJECT_ROOT, _resolve_path, load_command_toml
from robotnav.navigation.scene.artifact import file_sha256, load_scene_artifact


def verify_assets(manifest: dict) -> None:
    for relative, record in manifest["files"].items():
        path = _resolve_path(relative)
        if not path.is_file():
            raise FileNotFoundError(f"Missing bundled asset: {path}")
        if path.stat().st_size != record["size_bytes"] or file_sha256(path) != record["sha256"]:
            raise ValueError(f"Asset integrity check failed: {relative}")
    print(f"Assets: {len(manifest['files'])} files, all SHA-256 checks passed", flush=True)


def prepare_scene(manifest: dict, target: Path) -> None:
    from plyfile import PlyData, PlyElement

    chunks = []
    for relative in manifest["gaussian_subset"]["parts"]:
        with np.load(_resolve_path(relative), allow_pickle=False) as arrays:
            chunks.append(arrays["vertices"])
    vertices = np.concatenate(chunks)
    if len(vertices) != manifest["gaussian_subset"]["count"]:
        raise ValueError("Bundled Gaussian count differs from the asset manifest")
    target.parent.mkdir(parents=True, exist_ok=True)
    PlyData([PlyElement.describe(vertices, "vertex")], text=False, byte_order="<").write(target)
    print(f"Scene prepared: {len(vertices):,} real Gaussians -> {target}", flush=True)


def run_stage(module: str, *arguments: str) -> None:
    subprocess.run([sys.executable, "-m", module, *arguments], check=True, cwd=PROJECT_ROOT)


def render(animation_config: str, *, preview: bool, robot_only: bool) -> None:
    arguments = ["--config", animation_config]
    if preview:
        arguments.append("--preview-only")
    if robot_only:
        arguments.append("--robot-only")
    subprocess.run(
        [sys.executable, str(PROJECT_ROOT / "scripts/render_go2_scene_animation.py"), *arguments],
        check=True,
        cwd=PROJECT_ROOT,
    )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "command", choices=("check", "assets", "plan", "robot-preview", "preview", "render")
    )
    parser.add_argument("--config", default="demo.toml", help="Workflow TOML inside configs/")
    args = parser.parse_args()
    settings = load_command_toml(args.config, sections={"demo"})["demo"]
    manifest = json.loads(_resolve_path(settings["asset_manifest"]).read_text(encoding="utf-8"))
    verify_assets(manifest)
    if args.command == "check":
        from robotnav.navigation.trajectory.config import load_trajectory_generation_config

        subprocess.run([sys.executable, "-m", "pip", "check"], check=True)
        config = load_trajectory_generation_config(settings["trajectory_config"])
        scene = load_scene_artifact(config.paths.scene_dir)
        print(f"Scene map: {scene.model_sha256}; Python: {sys.executable}")
        ffmpeg = shutil.which("ffmpeg")
        if ffmpeg is None:
            raise FileNotFoundError("FFmpeg is required: install it before rendering video")
        print(f"FFmpeg: {ffmpeg}")
        try:
            from robotnav.gpu_environment import require_cuda_environment

            require_cuda_environment()
        except (RuntimeError, ImportError) as error:
            print(f"GPU rendering unavailable: {error}")
            print("CPU planning and robot-preview remain available.")
            raise SystemExit(1) from error
        print("GPU render checks passed")
        return
    if args.command in {"assets", "robot-preview", "preview", "render"}:
        prepare_scene(manifest, _resolve_path(settings["scene_cache"]))
    if args.command == "assets":
        return
    run_stage("robotnav.commands.generate_trajectories", "--config", settings["trajectory_config"])
    if args.command == "plan":
        return
    render(
        settings["animation_config"],
        preview=args.command != "render",
        robot_only=args.command == "robot-preview",
    )


if __name__ == "__main__":
    main()
