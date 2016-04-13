#!/bin/bash
if [ ! -d "config" ]; then
	mkdir config
fi
autoreconf --force --install -I config
