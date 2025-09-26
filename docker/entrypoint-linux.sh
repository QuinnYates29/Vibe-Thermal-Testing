#!/usr/bin/env bash

useradd -mu ${RUN_UID} -s /usr/bin/bash ${RUN_NAME}
sudo -iu ${RUN_NAME}
