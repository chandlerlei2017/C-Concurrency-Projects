# ECE252 Labs

## Lab 1 - Tools to operate on png files in a Linux Environment

findpng: A tool to find and list all png files within a given directory. Checks the header data of files and not just the extension

catpng: A tool to concatenate a sequence of png images together given in order as arguments

## Lab 2 - Multi-threaded png concatonation with cURL get requests

paster: A tool to extract 50 image parts from a web server using cURL and in multiple threads, then concatonate them based on sequence number given in the response header of the cURL requests

## Lab 3 - Multi-processed png concatenation with cURL requests

paster2: A tool to request specific images parts from a web surver using cURL in multiple processes, and then concatenate the images together in other processes.

## Lab 4 - Multi-threaded web crawler

findpng2: A tool that crawls a given seed URL, recursively following all other links found in each URL. The tool logs all URLs visited as well as the URLs of all png images found seperately. The tool terminates when all URLs have been visited from the seed URL or until a specified amount of png images is found.

