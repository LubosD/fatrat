# Build environment for the FatRat .deb packages, see build.sh
ARG UBUNTU_VERSION=26.10
FROM ubuntu:${UBUNTU_VERSION}
ARG DEBIAN_FRONTEND=noninteractive
# Node.js for the Angular web UI (the distributions' npm is too old for Angular 22)
ARG NODE_VERSION=24.15.0

RUN apt-get update && apt-get install -y --no-install-recommends \
		build-essential devscripts equivs ca-certificates curl xz-utils lintian \
	&& rm -rf /var/lib/apt/lists/*

RUN set -eux; \
	case "$(dpkg --print-architecture)" in amd64) a=x64;; arm64) a=arm64;; *) echo "unsupported architecture" >&2; exit 1;; esac; \
	curl -fsSL "https://nodejs.org/dist/v${NODE_VERSION}/node-v${NODE_VERSION}-linux-${a}.tar.xz" | tar -xJ -C /usr/local --strip-components=1

# Build dependencies come from debian/control, so the image does not have to be updated when they change
COPY debian/control /tmp/control
RUN apt-get update \
	&& mk-build-deps --install --remove --tool 'apt-get -y --no-install-recommends' /tmp/control \
	&& rm -rf /var/lib/apt/lists/* /tmp/control fatrat-build-deps_*
