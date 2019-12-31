FROM debian
COPY scripts/aca-start.sh /
RUN chmod u+x aca-start.sh
CMD ["./aca_start.sh"]
