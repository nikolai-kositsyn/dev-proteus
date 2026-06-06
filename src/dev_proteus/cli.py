#!/usr/bin/env python3
"""Command-line interface for dev-proteus"""

import argparse
import logging
import os
import signal
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from dev_proteus.core.emulator_server import *
from dev_proteus.utils.logger import APP_NAME, setup_logger


def main():
    """Main entry point"""
    parser = argparse.ArgumentParser(
        description='dev-proteus - Universal Linux Peripheral Emulator'
    )

    parser.add_argument(
        '-c', '--config',
        type=str,
        help='Path to JSON configuration file'
    )

    parser.add_argument(
        '--host',
        type=str,
        default=HOST_DEFAULT,
        help=f'Host to bind to (default: {HOST_DEFAULT})'
    )

    parser.add_argument(
        '--port',
        type=int,
        default=PORT_DEFAULT,
        help=f'Port to listen on (default: {PORT_DEFAULT})'
    )

    parser.add_argument(
        '--log-level',
        type=str,
        default="i",
        help='Set logging level'
    )

    args = parser.parse_args()

    # Setup logging
    log_levels = {
        "d": logging.DEBUG,
        "i": logging.INFO,
    }

    log_level = log_levels.get(args.log_level)
    logger = setup_logger(APP_NAME, log_level)

    if not args.config:
        logger.error("Configuration file is required (--config)")
        sys.exit(1)

    # Create and start server
    server = EmulatorServer(args.config, args.host, args.port)

    shutdown_event = threading.Event()

    def signal_handler(signum, frame):
        logger.info("Shutting down...")
        shutdown_event.set()

    signal.signal(signal.SIGINT, signal_handler)
    signal.signal(signal.SIGTERM, signal_handler)

    def server_runner():
        try:
            server.start()
        except Exception as e:
            logger.error(f"Server error: {e}")
            shutdown_event.set()

    thread = threading.Thread(target=server_runner,
                              name=f"{APP_NAME}-server",
                              daemon=True)
    thread.start()

    shutdown_event.wait()

    server.stop()
    thread.join(timeout=2.0)
    sys.exit(0)


if __name__ == "__main__":
    main()
