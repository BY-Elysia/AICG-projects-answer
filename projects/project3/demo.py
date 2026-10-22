"""Project-local launcher. No custom chat UI, tools, or agent execution loop."""
import argparse
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import tarfile
from importlib.resources import files

from dotenv import load_dotenv

ROOT = Path(__file__).resolve().parent
RUNTIME = ROOT / ".runtime" / "native"
WORKSPACE = ROOT / "workspace"
VERSION = "2.0.20"
BINARY = ROOT / ".runtime/bin/package/bin" / ("opencode.exe" if os.name == "nt" else "opencode")
SCA = Path(sys.executable).parent / ("sca.exe" if os.name == "nt" else "sca")


def environment():
    load_dotenv(ROOT / ".env")
    env = os.environ.copy()
    env["PATH"] = str(SCA.parent) + os.pathsep + env.get("PATH", "")
    for kind in ("CONFIG", "DATA", "CACHE", "STATE"):
        folder = RUNTIME / kind.lower()
        folder.mkdir(parents=True, exist_ok=True)
        env[f"XDG_{kind}_HOME"] = str(folder)
    return env


def configure():
    env = environment()
    for key in ("LLM_BASE_URL", "LLM_MODEL", "LLM_API_KEY"):
        if not env.get(key):
            raise SystemExit(f"请在项目 .env 中填写 {key}")
    config = {
        "providers": {"gateway": {
            "name": "Course model", "package": "@ai-sdk/openai-compatible",
            "env": ["LLM_API_KEY"], "settings": {"baseURL": env["LLM_BASE_URL"]},
            "models": {env["LLM_MODEL"]: {"modelID": env["LLM_MODEL"], "name": "CAD model"}},
        }},
        "model": "gateway/" + env["LLM_MODEL"],
        "skills": [str(ROOT / "skills")],
        "default_agent": "cad-demo",
        "agents": {"cad-demo": {"mode": "primary", "system": (ROOT / "AGENTS.md").read_text()}},
        "permissions": [
            {"action": "skill", "resource": "*", "effect": "allow"},
            {"action": "question", "resource": "*", "effect": "allow"},
        ],
    }
    folder = Path(env["XDG_CONFIG_HOME"]) / "opencode"
    folder.mkdir(parents=True, exist_ok=True)
    (folder / "opencode.json").write_text(json.dumps(config, ensure_ascii=False, indent=2))
    WORKSPACE.mkdir(exist_ok=True)
    return env


def install():
    if not BINARY.exists():
        npm = shutil.which("npm")
        if not npm:
            raise SystemExit("请先安装 Node.js（含 npm），参见 SETUP_GUIDE.md")
        system = {"Darwin": "darwin", "Linux": "linux", "Windows": "windows"}[platform.system()]
        arch = {"arm64": "arm64", "aarch64": "arm64", "x86_64": "x64", "AMD64": "x64"}[platform.machine()]
        target = BINARY.parents[2]
        target.mkdir(parents=True, exist_ok=True)
        result = subprocess.run([
            npm, "pack", f"@opencode/cli-{system}-{arch}@{VERSION}", "--json",
            "--cache", str(ROOT / ".cache/npm"), "--pack-destination", str(target),
        ], check=True, text=True, capture_output=True)
        archive_path = target / json.loads(result.stdout)[0]["filename"]
        with tarfile.open(archive_path) as archive:
            archive.extractall(target, filter="data")
    subprocess.run([str(BINARY), "--version"], check=True)
    subprocess.run([str(SCA), "skill", "install", "--target", "opencode",
                    "--skills-dir", str(ROOT / "skills"), "--force"], check=True)
    WORKSPACE.mkdir(exist_ok=True)
    example = WORKSPACE / "u_link"
    if not example.exists():
        source = files("simplecadapi").joinpath("example/resources/u_link_motor_mount")
        shutil.copytree(str(source), example, ignore=shutil.ignore_patterns("__pycache__"))
    print("已安装官方 Skill 和原生示例。两个终端分别运行 demo.py editor 与 demo.py agent。")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=["install", "configure", "editor", "agent"])
    args = parser.parse_args()
    if args.command == "install":
        install()
    elif args.command == "configure":
        configure()
    elif args.command == "editor":
        WORKSPACE.mkdir(exist_ok=True)
        subprocess.run([str(SCA), "edit", str(WORKSPACE), "--host", "127.0.0.1",
                        "--port", "8765", "--no-browser"], env=environment(), check=True)
    else:
        if not BINARY.exists() or not (ROOT / "skills/simplecadapi/SKILL.md").exists():
            raise SystemExit("请先执行 uv run python demo.py install")
        subprocess.run([str(BINARY), "--standalone", str(WORKSPACE)],
                       cwd=WORKSPACE if WORKSPACE.exists() else ROOT, env=configure(), check=True)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
