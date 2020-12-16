Top-level regression test directory.  Driven by `make regress` in the source
root.

These regressions are used for a preliminary test of ort(1) to see if it
produces correct results.  They're also by some front-ends to validate
their output given corner conditions of the input configuration.

Language-specific regression tests are in subdirectories. 

There are two types of file in this directory:

1. Test files ending in .nresult should fail compilation.
2. Text files ending in .result have their corresponding .ort files
   translated and the output compared.  They must match.
