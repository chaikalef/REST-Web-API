test:
	clear
	make rest
	make default
	make t_utc_nix
	make t_utc_inet
	make t_local_nix
	make t_local_inet
	make post
	make clean
rest: rest.o
	gcc rest.o -o rest
rest.o: rest.c
	gcc -c rest.c
clean:     
	rm rest.o rest
default:
	./rest
t_utc_nix:
	./rest -t utc -f unix
t_utc_inet:
	./rest -t utc -f internet
t_local_nix:
	./rest -t local -f unix
t_local_inet:
	./rest -t local -f internet
post:
	./rest -p
