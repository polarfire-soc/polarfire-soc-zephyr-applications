FROM ubuntu:20.04

ARG ZSDK_VERSION=0.12.4
ARG ZSDK_ALT_VERSION=0.13.0-rc1
ARG CMAKE_VERSION=3.18.3
ARG LLVM_VERSION=12
ARG WGET_ARGS="-q --show-progress --progress=bar:force:noscroll --no-check-certificate"

ARG UID=1000
ARG GID=1000

ENV DEBIAN_FRONTEND noninteractive

RUN	apt-get -y update && \
	apt-get -y upgrade && \
	apt-get install --no-install-recommends -y \
    lsb-release \
	autoconf \
	automake \
	bison \
	build-essential \
	ccache \
	device-tree-compiler \
	dfu-util \
	dos2unix \
	doxygen \
	file \
	flex \
	g++ \
	gawk \
	gcc \
	gcc-multilib \
	g++-multilib \
	gcovr \
	git \
	git-core \
	gperf \
	gtk-sharp2 \
	help2man \
	iproute2 \
	lcov \
	libglib2.0-dev \
	libgtk2.0-0 \
	liblocale-gettext-perl \
	libncurses5-dev \
	libpcap-dev \
	libpopt0 \
	libsdl1.2-dev \
	libsdl2-dev \
	libssl-dev \
	libtool \
	libtool-bin \
	locales \
	make \
	net-tools \
	ninja-build \
	openssh-client \
	pkg-config \
	protobuf-compiler \
	python3-dev \
	python3-pip \
	python3-ply \
	python3-setuptools \
	python-is-python3 \
	qemu \
	rsync \
	socat \
	srecord \
	sudo \
	texinfo \
	unzip \
	valgrind \
	wget \
	xz-utils && \
    rm -rf /var/lib/apt/lists/* -rf /var/lib/apt/lists/*


#RUN locale-gen en_US.UTF-8
#ENV LANG en_US.UTF-8
#ENV LANGUAGE en_US:en
#ENV LC_ALL en_US.UTF-8

RUN pip3 install wheel pip -U &&\
	pip3 install -r https://raw.githubusercontent.com/zephyrproject-rtos/zephyr/master/scripts/requirements.txt && \
	pip3 install west &&\
	pip3 install sh &&\
	pip3 install awscli PyGithub junitparser pylint \
		     statistics numpy \
		     imgtool \
		     protobuf


RUN mkdir -p /opt/toolchains


RUN wget ${WGET_ARGS} https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-Linux-x86_64.sh && \
	chmod +x cmake-${CMAKE_VERSION}-Linux-x86_64.sh && \
	./cmake-${CMAKE_VERSION}-Linux-x86_64.sh --skip-license --prefix=/usr/local && \
	rm -f ./cmake-${CMAKE_VERSION}-Linux-x86_64.sh



RUN wget ${WGET_ARGS} https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${ZSDK_VERSION}/zephyr-sdk-${ZSDK_VERSION}-x86_64-linux-setup.run && \
	sh "zephyr-sdk-${ZSDK_VERSION}-x86_64-linux-setup.run" --quiet -- -d /opt/toolchains/zephyr-sdk-${ZSDK_VERSION} && \
	rm "zephyr-sdk-${ZSDK_VERSION}-x86_64-linux-setup.run"


RUN groupadd -g $GID -o user

RUN useradd -u $UID -m -g user -G plugdev user \
	&& echo 'user ALL = NOPASSWD: ALL' > /etc/sudoers.d/user \
	&& chmod 0440 /etc/sudoers.d/user

# Set the locale
ENV ZEPHYR_TOOLCHAIN_VARIANT=zephyr
ENV ZEPHYR_SDK_INSTALL_DIR=/opt/toolchains/zephyr-sdk-${ZSDK_VERSION}
ENV PKG_CONFIG_PATH=/usr/lib/i386-linux-gnu/pkgconfig

USER user
