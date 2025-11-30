make:
    cmake build -B build -S .

render_writeup:
    cd writeup && pandoc --from markdown --to pdf writeup.md -o writeup.pdf -V geometry:margin=1in