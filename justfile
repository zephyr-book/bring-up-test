alias c := clean
alias b := build
alias f := flash

openocd_bin := "~/.local/bin/usr/local/bin/openocd"
openodc_dir := "~/.local/openocd"

clean:
    rip build

# P1 is the default revision; P2 needs the @p2 suffix.
build:
    west build -p always -b zbook/rp2350b/m33

build-p2:
    west build -p always -b "zbook@p2/rp2350b/m33"

build-wifi:
    west build -p always -b zbook/rp2350b/m33 --shield zbook_wifi

build-p2-wifi:
    west build -p always -b "zbook@p2/rp2350b/m33" --shield zbook_wifi

flash:
    west flash --openocd {{ openocd_bin }} --openocd-search {{ openodc_dir }}
