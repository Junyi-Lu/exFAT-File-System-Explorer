## how to compile files:

make

## how to run for exfat:
### my program should take blow command-line arguments:

argv[0] for ./exfat


argv[1] for image file name


argv[2] for command type (info,list,get)


argv[3] only valid for get command, it include the path, u could follow the example operations


## for example:
### for info command:


./exfat a4image.exfat info

### for list command:


./exfat a4image.exfat list

### for get command:


(dont forget the quote cover the file name)


./exfat a4image.exfat get dirs/in/dirs/in/dirs/"greetings.txt" 


./exfat a4image.exfat get ebooks/"h-g-wells_the-time-machine.epub"


./exfat a4image.exfat get "README.md"


/exfat a4image.exfat get text/numbers/"11.txt"



## how to clean object and output file :

make clean