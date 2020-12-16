Test syntax of files the parent directory by passing them from
ort-nodejs(1) into the transpiler.

To save on the tremendous time taken to start up Node, this occurs
within regress-runner.ts, which does this all within a Node instance.
