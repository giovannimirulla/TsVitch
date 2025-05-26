# 构建 NSP forwarder （桌面图标）

1. Compile using make
    ```shell
    make
    make TsVitch.nacp
   ```
2. hacbrewpack 打包
   Create a directory named switch-hacbrewpack and place your device key in it, naming the file prod.keys.
   ```shell
   mv build/exefs/main switch-hacbrewpack/exefs/main
   mv build/exefs/main.npdm switch-hacbrewpack/exefs/main.npdm
   mv TsVitch.nacp switch-hacbrewpack/control/control.nacp
   cp ../../resources/icon/icon.jpg switch-hacbrewpack/control/icon_AmericanEnglish.dat
   
   cd switch-hacbrewpack
   hacbrewpack -k prod.keys --titleid 010ff000ffff0021 --titlename TsVitch --noromfs --nologo
   ```

# 感谢

1. https://github.com/switchbrew/nx-hbloader
2. https://github.com/The-4n/hacBrewPack
