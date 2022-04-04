# create the test_files directory if needed
mkdir -p test_files
cd test_files

# generate some random data of size 512 KB, 1 MB, 16 MB, 128 MB, 512 MB
dd if=/dev/urandom of=512KB.dat bs=512K count=1
dd if=/dev/urandom of=1MB.dat bs=1M count=1
dd if=/dev/urandom of=16MB.dat bs=1M count=16
dd if=/dev/urandom of=128MB.dat bs=1M count=128
dd if=/dev/urandom of=512MB.dat bs=1M count=512

# fetch some cool space images
wget -O earth.jpg images-assets.nasa.gov/image/PIA18033/PIA18033~orig.jpg
wget -O moon.jpg images-assets.nasa.gov/image/as16-113-18339/as16-113-18339~orig.jpg
wget -O orion.mp4 images-assets.nasa.gov/video/NHQ_2014_1107_TWAN/NHQ_2014_1107_TWAN~orig.mp4

# fetch romeo and juliet
wget -O romeo_and_juliet.txt http://www.textfiles.com/etext/AUTHORS/SHAKESPEARE/shakespeare-romeo-48.txt