sipp-prerequisites:
  pkg.installed:
    - pkgs:
      - dh-autoreconf
      - ncurses-dev
      - build-essential
      - libssl-dev
      - libpcap-dev
      - libncurses5-dev
      - libsctp-dev
      - lksctp-tools


# /usr/local/src/sipp:
#   file.recurse:
#     - source: salt://sipp/sipp-3.4.1
#     - makedirs: True
#     - include_empty: True


sipp-git:
  git.latest:
    - name: https://github.com/SIPp/sipp.git
    - target: /opt/sipp
    - rev: master
  cmd.wait:
    - name: ./build.sh --with-pcap --with-sctp --with-openssl
    - cwd: /opt/sipp
    - watch:
      - git: sipp-git

export PATH=/opt/sipp/:$PATH:
  cmd.run