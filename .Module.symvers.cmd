cmd_/media/shared/AOS/Module.symvers := sed 's/\.ko$$/\.o/' /media/shared/AOS/modules.order | scripts/mod/modpost -m -a  -o /media/shared/AOS/Module.symvers -e -i Module.symvers   -T -
