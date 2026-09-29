# Ubuntu 22.04 is a tested host distro for Yocto 5.0 (scarthgap).
FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive \
    LANG=en_US.UTF-8 \
    LC_ALL=en_US.UTF-8 \
    LANGUAGE=en_US:en

# Host packages required by the Yocto 5.0 reference manual, plus bmap-tools
# to write the generated .wic.bz2 onto an SD card.
RUN apt-get update && apt-get install -y --no-install-recommends \
        gawk wget git diffstat unzip texinfo gcc build-essential \
        chrpath socat cpio python3 python3-pip python3-pexpect \
        python3-git python3-jinja2 python3-subunit python3-distutils \
        xz-utils debianutils iputils-ping python3-venv \
        zstd liblz4-tool file locales libacl1 \
        libegl1-mesa libsdl1.2-dev mesa-common-dev \
        ca-certificates curl sudo bmap-tools \
        parted dosfstools \
    && sed -i 's/^# *\(en_US.UTF-8 UTF-8\)/\1/' /etc/locale.gen \
    && locale-gen en_US.UTF-8 \
    && git config --system --add safe.directory '*' \
    && rm -rf /var/lib/apt/lists/*

ARG UID=1000
ARG GID=1000
RUN groupadd -g "${GID}" builder \
    && useradd -m -u "${UID}" -g "${GID}" -s /bin/bash builder \
    && echo 'builder ALL=(ALL) NOPASSWD:ALL' > /etc/sudoers.d/builder \
    && chmod 0440 /etc/sudoers.d/builder

COPY entrypoint.sh yocto-init.sh flash-sd.sh layers.lock /opt/yocto/
RUN chmod 0755 /opt/yocto/entrypoint.sh /opt/yocto/yocto-init.sh /opt/yocto/flash-sd.sh \
    && ln -sf /opt/yocto/flash-sd.sh /usr/local/bin/flash-sd

USER builder
WORKDIR /work/yocto

ENTRYPOINT ["/opt/yocto/entrypoint.sh"]
CMD ["bash"]
