#!/usr/bin/env python3
import argparse
import os
import posixpath
import sys

import paramiko


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", required=True)
    parser.add_argument("--archive", required=True)
    parser.add_argument("--user", default="root")
    parser.add_argument("--password-env", default="POWERCLAW_SSH_PASSWORD")
    args = parser.parse_args()
    password = os.environ.get(args.password_env)
    if not password:
        raise SystemExit(f"missing environment variable {args.password_env}")

    archive = os.path.abspath(args.archive)
    remote_archive = "/tmp/powerclaw-deploy.tar.gz"
    client = paramiko.SSHClient()
    client.set_missing_host_key_policy(paramiko.AutoAddPolicy())
    client.connect(args.host, username=args.user, password=password, timeout=10)
    try:
        sftp = client.open_sftp()
        try:
            sftp.put(archive, remote_archive + ".part")
            sftp.rename(remote_archive + ".part", remote_archive)
        finally:
            sftp.close()
        command = (
            "set -e; rm -rf /tmp/powerclaw-deploy; mkdir /tmp/powerclaw-deploy; "
            f"tar -xzf {remote_archive} -C /tmp/powerclaw-deploy; "
            "/tmp/powerclaw-deploy/powerclaw/install-board.sh; "
            f"rm -rf /tmp/powerclaw-deploy {remote_archive}"
        )
        _, stdout, stderr = client.exec_command(command, timeout=30)
        output = stdout.read().decode("utf-8", "replace")
        errors = stderr.read().decode("utf-8", "replace")
        status = stdout.channel.recv_exit_status()
        sys.stdout.write(output)
        sys.stderr.write(errors)
        if status:
            raise SystemExit(status)
    finally:
        client.close()


if __name__ == "__main__":
    main()
