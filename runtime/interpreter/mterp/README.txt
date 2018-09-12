==== Overview ====

The assembly source code is produced from custom python-based templates.
All the architecture-specific template files are concatenated to create
one big python script. This generated python script is then executed to
produced the final assembly file. The template syntax is:
 * Lines starting with % are python code. They will be copied as-is to
   the script (without the %) and thus executed during the generation.
 * Other lines are text, and they are essentially syntax sugar for
   out.write('''(line text)''') and thus they write the main output.
 * Within a text line, $ can be used insert variables from code.

The final assembly sources are written into the "out" directory, where
they are picked up by the Android build system.

The best way to become familiar with the interpreter is to look at the
generated files in the "out" directory.


==== Instruction file format ====

The assembly instruction files are simply fragments of assembly sources.
The starting label will be provided by the generation tool, as will
declarations for the segment type and alignment.

The following global variables are generally available:

  $opcode - opcode name, e.g. "OP_NOP"
  $opnum - opcode number, e.g. 0 for OP_NOP
  $handler_size_bytes - max size of an instruction handler, in bytes
  $handler_size_bits - max size of an instruction handler, log 2

Both C and assembly sources will be passed through the C pre-processor,
so you can take advantage of C-style comments and preprocessor directives
like "#define".

The generation tool does *not* print a warning if your instructions
exceed "handler-size", but the VM will abort on startup if it detects an
oversized handler.  On architectures with fixed-width instructions this
is easy to work with, on others this you will need to count bytes.


==== Using C constants from assembly sources ====

The file "art/runtime/asm_support.h" has some definitions for constant
values, structure sizes, and struct member offsets.  The format is fairly
restricted, as simple macros are used to massage it for use with both C
(where it is verified) and assembly (where the definitions are used).

If a constant in the file becomes out of sync, the VM will log an error
message and abort during startup.


==== Rebuilding ====

If you change any of the source file fragments, you need to rebuild the
combined source files in the "out" directory.  Make sure the files in
"out" are editable, then:

    $ cd mterp
    $ ./gen_mterp.py

The ultimate goal is to have the build system generate the necessary
output files without requiring this separate step, but we're not yet
ready to require Python in the build.

==== Interpreter Control ====

The mterp fast interpreter achieves much of its performance advantage
over the C++ interpreter through its efficient mechanism of
transitioning from one Dalvik bytecode to the next.  Mterp for ARM targets
uses a computed-goto mechanism, in which the handler entrypoints are
located at the base of the handler table + (opcode * 128).

In normal operation, the dedicated register rIBASE
(r8 for ARM, edx for x86) holds a mainHandlerTable.  If we need to switch
to a mode that requires inter-instruction checking, rIBASE is changed
to altHandlerTable.  Note that this change is not immediate.  What is actually
changed is the value of curHandlerTable - which is part of the interpBreak
structure.  Rather than explicitly check for changes, each thread will
blindly refresh rIBASE at backward branches, exception throws and returns.
