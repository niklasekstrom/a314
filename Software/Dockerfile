FROM toarnold/vbcc
ENV VBCC /opt/vbccdev/
RUN apk update
RUN apk add python lha
RUN wget www.haage-partner.de/download/AmigaOS/NDK39.lha -O - | lha x -w=/opt/vbccdev/ - 
