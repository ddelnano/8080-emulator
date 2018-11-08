This is my implementation of emulator101.com

## Testing

```bash
$ make test
```

This will use a 8080 binary that will try to test many opcodes.  It's not a perfect test but can catch bugs in many of the opcodes.

## Running

For now this only runs on OS X, in the future maybe this will work on the web with web assembly or on Linux.

### Running on OSX
You can run the project with the osx target.

```bash
make osx
```

You can run the application under lldb if you set the DEBUG variable to true.

```bash
DEBUG=true make osx
```
