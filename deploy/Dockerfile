FROM gcc:13 AS build
WORKDIR /src
COPY . .
RUN make clean && make all && make test

FROM debian:bookworm-slim AS runtime
RUN useradd -m fve
WORKDIR /app
COPY --from=build /src/bin/server /app/bin/server
COPY --from=build /src/bin/verify /app/bin/verify
COPY --from=build /src/bin/extract /app/bin/extract
COPY --from=build /src/ui /app/ui
COPY --from=build /src/db /app/db
RUN mkdir -p /app/data && chown -R fve /app
USER fve
EXPOSE 8080
ENV FVE_DATA=/app/data FVE_UI=/app/ui FVE_PORT=8080
CMD ["/bin/sh","-c","/app/bin/server $FVE_DATA $FVE_UI $FVE_PORT"]
