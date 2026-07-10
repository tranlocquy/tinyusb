#!/usr/bin/env python

import argparse

parser = argparse.ArgumentParser(description="Generate cangen calls for ASCII commands")
parser.add_argument("cmds", metavar="CMD", help="command to run", nargs="*")
parser.add_argument("-c", "--can", metavar="CAN", help="CAN device", default="can0")

args = parser.parse_args()

for cmd in args.cmds:
	cmd_len = len(cmd)

	for i in range(0, cmd_len, 8):
		frame_len = min(cmd_len - i, 8)
		data = cmd[i:i+frame_len].encode("ASCII").hex()

		print(f"cangen -x -n 1 -I 0 -L {frame_len} -D {data} {args.can}")

	if (cmd_len % 8) == 0:
		# send ZLP
		print(f"cangen -x -n 1 -I 0 -L 0 {args.can}")
