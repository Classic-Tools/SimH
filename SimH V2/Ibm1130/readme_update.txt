Interim 1130 distribution:
--------------------------------------------

folders:
	.		sources
	winrel		windows executables
	windebug	windows executables
        utils           accessory programs
	utils\winrel	windows executables
	utils\windebug	windows executables
	sw		working directory for DMS build & execution
	sw\dmsR2V12	Disk Monitor System sources

programs:
	asm1130		cross assembler
	bindump		object deck dump tool, also used to sort decks by phase id
	checkdisk	DMS disk image check and dump
	diskview	work in progress, interpreted disk image dump
	ibm1130		emulator
	mkboot		object deck to IPL and core image converter
	viewdeck	binary to hollerith deck viewer if needed to view phase ID cards and ident fields

batch file:
	mkdms.bat	builds DMS objects and binary cards. Need a shell script version of this.

IBM1130 simulator DO command scripts:
	format		format a disk image named DMS.DSK
	loaddms		format and install DMS onto the formatted DMS.DSK
        for             run a Fortran program
        list            list the disk contents
        asm             assemble a program

ancillary files:
	loaddms.deck	list of files stacked into the card reader for loaddms
        *.deck          other sample deck files
