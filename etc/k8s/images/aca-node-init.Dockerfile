FROM debian
COPY scripts/aca-node-init.sh /
RUN chmod u+x aca-node-init.sh
CMD ["./aca-node-init.sh"]
