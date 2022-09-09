## Project Details

### Implementation

Refer to the report PDF in the repository for all the required details.

### Execution

The Makefile consists of the -m32 GCC flag which allows compilation for a 32 bit system (though, even if this flag is removed, the code will still work the same on a 64 bit system too).
</br>
The \_GNU\_SOURCE GCC flag has been primarily added for enabling the use of register enums defined in the ucontext header file (see #defines in the psu\_thread.h file for more details).

For executing an app at the server (do this before executing at the client)-

```
./app<num> <client_ip> 1
```

For executing the same app at the client-

```
./app<num> <server_ip> 0
```
