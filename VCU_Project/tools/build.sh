#! /bin/bash

#Assuming installation is in /opt
/opt/ti/ccs2041/ccs/eclipse/ccs-server-cli.sh -noSplash -workspace "/home/dylan/workspace_ccstheia" -application projectImport -ccs.location "/home/dylan/stuff/Firmware/VCU_Project"
/opt/ti/ccs2041/ccs/eclipse/ccs-server-cli.sh -noSplash -workspace "/home/dylan/workspace_ccstheia" -application projectBuild -ccs.projects "VCU_Project"   