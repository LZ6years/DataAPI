all: libpublic.a libpublic.so libftp.a libftp.so

libpublic.a:_public.h _public.cpp
	g++ -fPIC -c -o libpublic.a _public.cpp
	mv libpublic.a ../lib/libpublic.a 

libpublic.so:_public.h _public.cpp
	g++ -fPIC -shared -o libpublic.so _public.cpp 
	mv libpublic.so ../lib/libpublic.so

libftp.a:ftplib.h ftplib.c
	gcc -c -fPIC -o libftp.a ftplib.c 
	mv libftp.a ../lib/libftp.a

libftp.so:ftplib.h ftplib.c
	gcc -fPIC -shared -o libftp.so ftplib.c 
	mv libftp.so ../lib/libftp.so

clean:
	rm -f libpublic.a libpublic.so libftp.a libftp.so
