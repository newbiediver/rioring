FROM ubuntu:22.04
RUN apt update && apt install -y git cmake g++-11 ninja-build liburing-dev
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 900 --slave /usr/bin/g++ g++ /usr/bin/g++-11
RUN mkdir -p /tmp/rioring && git clone https://github.com/newbiediver/rioring.git /tmp/rioring
RUN cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local -G Ninja -S /tmp/rioring -B /tmp/rioring/build && cmake --build /tmp/rioring/build --target rioring
RUN cmake --install /tmp/rioring/build/src
RUN rm -rf /tmp/rioring
