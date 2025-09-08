# Goldin Programming language

## Description
Goldin is toy programming language built for educational purposes 
that compiles directly to x86_64 native linux assembly.

## Dependencies
- clang

## Testing
If you want to try out the project do the following commands:

```
    make
    ./build/au -o hello_world examples/hello_world.gdn
    ./hello_world
```

To clean all the garbage the compiler produced:

```
    git clean -fdx
```
