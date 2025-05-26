
make
make tsvitch.nacp

mv build/exefs/main switch-hacbrewpack/exefs/main
mv build/exefs/main.npdm switch-hacbrewpack/exefs/main.npdm
mv tsvitch.nacp switch-hacbrewpack/control/control.nacp
cp ../../resources/icon/logo.png switch-hacbrewpack/logo/NintendoLogo.png
cp ../../resources/icon/icon.jpg switch-hacbrewpack/control/icon_AmericanEnglish.dat

cd switch-hacbrewpack || exit
hacbrewpack -k prod.keys --titleid 010ff000ffff0021 --titlename tsvitch --noromfs