"""Validated TOML configuration shared by RobotNav commands."""

from __future__ import annotations

from dataclasses import MISSING, fields
from pathlib import Path
from typing import Any

PROJECT_ROOT = Path(__file__).resolve().parents[2]
CONFIG_DIR = PROJECT_ROOT / "configs"


class ConfigurationError(ValueError):
    """Raised when a configuration file does not match its schema."""


def load_toml(path: Path) -> dict[str, Any]:
    """Read one TOML document."""
    if not path.is_file():
        raise FileNotFoundError(path)
    import tomli as tomllib

    with path.open("rb") as file:
        return tomllib.load(file)


def load_command_toml(filename: str, *, sections: set[str]) -> dict[str, Any]:
    """Load one command TOML and reject misspelled sections early."""
    path = CONFIG_DIR / filename
    if path.parent != CONFIG_DIR or path.suffix != ".toml":
        raise ConfigurationError(f"Expected a TOML file directly in {CONFIG_DIR}: {filename}")
    raw = load_toml(path)
    missing = sections - raw.keys()
    unknown = raw.keys() - sections
    if missing or unknown:
        details = []
        if missing:
            details.append("missing sections: " + ", ".join(sorted(missing)))
        if unknown:
            details.append("unknown sections: " + ", ".join(sorted(unknown)))
        raise ConfigurationError(f"Invalid {filename} ({'; '.join(details)})")
    return raw


def load_dataclass_section(raw: dict[str, Any], section: str, schema: type[Any]) -> dict[str, Any]:
    """Validate TOML keys against a dataclass while retaining its defaults."""
    values = raw[section]
    if not isinstance(values, dict):
        raise ConfigurationError(f"[{section}] must be a TOML table")
    declared = {field.name for field in fields(schema)}
    required = {
        field.name
        for field in fields(schema)
        if field.default is MISSING and field.default_factory is MISSING
    }
    missing = required - values.keys()
    unknown = values.keys() - declared
    if missing or unknown:
        details = []
        if missing:
            details.append("missing keys: " + ", ".join(sorted(missing)))
        if unknown:
            details.append("unknown keys: " + ", ".join(sorted(unknown)))
        raise ConfigurationError(f"Invalid [{section}] ({'; '.join(details)})")
    return values


def _resolve_path(value: str) -> Path:
    path = Path(value)
    return path if path.is_absolute() else PROJECT_ROOT / path
