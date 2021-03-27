FROM ghcr.io/toltec-dev/toolchain:v1.4

ARG USERNAME=yves
ARG USER_UID=1000
ARG USER_GID=$USER_UID

# add scp / ssh
RUN export DEBIAN_FRONTEND=noninteractive \
    && apt-get update -y \
    && apt-get install -y --no-install-recommends \
        openssh-client

# Create the user
RUN groupadd --gid $USER_GID $USERNAME \
    && useradd --uid $USER_UID --gid $USER_GID -m $USERNAME

   