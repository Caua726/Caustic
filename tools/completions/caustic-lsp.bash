# caustic-lsp — bash completion.
#
# lsp/main.cst declares `fn main() as i32` — no argc, no argv. The server speaks
# JSON-RPC over stdio and is launched by an editor, not by hand, so the honest
# completion is to offer nothing at all. Without this, bash would fall back to
# completing filenames and suggest arguments the binary cannot read.
#
# install:  <prefix>/share/bash-completion/completions/caustic-lsp  (loaded on demand)
#     or:   . tools/completions/caustic-lsp.bash                    (from ~/.bashrc)

_caustic_lsp() {
    COMPREPLY=()
    return 0
}

# The toolchain ships under several names: a plain ELF, a Windows .exe, and
# the CSE flavours (.cse / .cse.exe) that carry every OS and architecture in
# one file. All of them are the same program and take the same arguments.
complete -F _caustic_lsp caustic-lsp
complete -F _caustic_lsp caustic-lsp.exe
complete -F _caustic_lsp caustic-lsp.cse
complete -F _caustic_lsp caustic-lsp.cse.exe
complete -F _caustic_lsp caustic-lsp-universal.cse
complete -F _caustic_lsp caustic-lsp-universal.cse.exe
