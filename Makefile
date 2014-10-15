ccflags-y+=-Werror
ccflags-y+=-I$(src)/include

obj-m += pcsc_passthru.o

pcsc_passthru-objs:=   \
		driver.o reader.o
