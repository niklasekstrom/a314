FROM dtcooper/raspberrypi-os:bookworm

RUN apt-get update && apt-get install -y build-essential libraspberrypi-dev raspberrypi-kernel-headers

COPY bootstrap.sh /root/bootstrap.sh
RUN apt install -y p7zip-full wget sudo    && \
    /root/bootstrap.sh                     && \
    apt remove -y  p7zip-full wget         && \
    apt autoremove -y

ENV VBCC=/opt/vbcc
ENV PATH=$PATH:$VBCC/bin
ENV NDK32=/opt/ndk32
