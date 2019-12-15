# FileTransfer
Make sure you have gcc before building the binary files to run the programs.

To build the client, go to `FileTransfer/Client/`, do `make` and do `./tcp_client`.

To build the server, go to `FileTransfer/Server/`, do `make` and do `./tcp_server`.

Both can be deleted by running `make clean`.

## Client Functions
### Upload
Client can use `upload$<FILENAME>` to upload a file to the server named `<FILENAME>`.
### Download
Client can use `download$<FILENAME>` to download a file from server called `<FILENAME>`.
### Exit
Type `exit` to exit the program.

Server does not have commands.
