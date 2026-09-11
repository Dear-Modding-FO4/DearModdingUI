# Shared diagnostic tooling

This directory owns the interactive diagnostic suite used by both the
standalone preview and the F4SE test client. The runners provide only their
environment adapters; they do not carry separate copies of the exercises.

`include/` contains narrow compatibility stubs required while production
sources are compiled into tooling and automated-test targets. They preserve
the production include names intentionally and are not external SDK headers.
Only desktop preview and automated tests use this include root. Game plugins and the F4SE test client
must use real logging and DLL discovery instead.
