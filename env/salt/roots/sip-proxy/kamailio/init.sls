
kamailio-prerequisites:
  pkg.installed:
    - pkgs:
      - mysql-server

kamailio-deb:
  pkgrepo.managed:
    - humanname: Kamailio PPA
    - name: deb http://deb.kamailio.org/kamailio trusty main
    - dist: trusty
    - file: /etc/apt/sources.list.d/kamailio.list
    - keyid: fb40d3e6508ea4c8
    - keyserver: keyserver.ubuntu.com
    - require_in:
      - pkg: kamailio

kamailio:
  pkg.latest:
    - pkgs:
      - kamailio
      - kamailio-mysql-modules
      - kamailio-unixodbc-modules
      - kamailio-json-modules
      - kamailio-kazoo-modules
      - kamailio-tls-modules
      - kamailio-websocket-modules
      - kamailio-extra-modules
    - refresh: True
    # - require:
    #   - file: /usr/local/bin/postinstall.sh

# /usr/local/bin/postinstall.sh:
#   cmd.wait:
#     - watch:
#       - pkg: kamailio
#   file.managed:
#     - source: salt://kamailio/postinstall.sh
#     - mode: '0777'

/etc/default/kamailio:
  file.managed:
    - source: salt://kamailio/conf/etc_default_kamailio

