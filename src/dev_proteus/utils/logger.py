"""Logging utilities"""

import logging
import sys

APP_NAME = "dev-proteus"

LOG_FORMAT = f"%(asctime)s.%(msecs)03d [%(levelname)s] [%(threadName)-9s] %(message)s"
LOG_TIMESTAMP_FORMAT = "%Y-%m-%d %H:%M:%S"


def setup_logger(name: str, level: int = logging.INFO) -> logging.Logger:
    """Setup logger with console handler"""

    logger = logging.getLogger(name)
    logger.setLevel(level)

    if not logger.handlers:
        handler = logging.StreamHandler(sys.stdout)
        handler.setLevel(level)

        formatter = logging.Formatter(LOG_FORMAT, LOG_TIMESTAMP_FORMAT)
        handler.setFormatter(formatter)

        logger.addHandler(handler)

    return logger


def get_logger(name: str = APP_NAME) -> logging.Logger:
    """Get default logger"""
    return logging.getLogger(name=name)
