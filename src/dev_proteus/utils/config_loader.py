"""Configuration loader for JSON files"""

import json
from typing import Dict, Any


def load_config(config_path: str) -> Dict[str, Any]:
    """Load configuration from JSON file"""
    with open(config_path, 'r', encoding='utf-8') as f:
        return json.load(f)


def save_config(config: Dict[str, Any], config_path: str) -> None:
    """Save configuration to JSON file"""
    with open(config_path, 'w', encoding='utf-8') as f:
        json.dump(config, f, indent=2, ensure_ascii=False)
