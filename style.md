# AirBitz C++ Coding Style

The AirBitz core was originally written in C.
Although everything has been converted to C++,
the source code is still very C-like.
As AirBitz moves forward with core refactoring,
much of this code will need to move off the old C idioms
and adopt a more C++ way of doing things.

The goal is to enable a gradual transition
while remaining respectful towards the old C codebase.

## Naming

New C++ classes and functions should go in the `abcd` namespace.
The code should generally avoid `using` statements,
so things should be spelled with their full names, such as `std::string`.

Functions and variables should use the camelCase convention.
Acronyms should be capitalized as normal words, like `getUrl` or `pinLogin`.

Old C variable names tend to include prefixes, such as `paszWalletNames`.
These prefixes should not appear in C++ code,
since the type system is much stronger.

Classes should begin with an upper-case character, like `PaymentRequest`.
This corresponds to what Java does, and is different from libbitcoin.
Member variables have a trailing underscore.

Macros don't have namespaces,
so we continue to use the old `ABC` prefix, like `ABC_ERROR`.

New files should be named after the C++ class they implement.
Headers should use the `.hpp` extension.

Read accessors should just be the name of the thing, like `name`.
Write accessors should be the name of the thing followed by a verb,
like `nameSet` or `nameClear`. Immutable members are always preferable,
so write accessors should be rare.

## Spacing

Indentation is 4 spaces (no tabs)

The `*` and `&` operators attach to the name, not to the type.

## Include order

Include statements should occur in the following order:

* The matching header for the current source file
* ABC headers
* Non-standard outside libraries
* C standard library headers
* C++ standard library headers

This order maximizes the chances of detecting errors in the dependency graph,
since the files that are most likely to have problems come first.

## Comments

Each file should begin with a license block.
The license block is not a Doxygen comment,
so it starts with `/*` instead of `/**`.

Header files should also have a Doxygen file header (`/** @file`)
immediately after the comment block.
Source files do not need `@file` comments,
since their matching header contains the necessary information.

## Automatic formatting

Please format the source code using [astyle](http://astyle.sourceforge.net/)
along with the options provided in the `astyle-options` file.

This can be done easily by invoking `make format`.

### Formatting git hook

For convenience, enable a Git hook to trigger the astyle styling whenever a git commit operation is performed.
This is done by typing in the repository directory:

    cd .git/hooks && ln -s ../../util/git-hooks/pre-commit
