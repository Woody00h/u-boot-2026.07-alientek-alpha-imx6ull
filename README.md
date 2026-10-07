# U-Boot 2026.07：ALIENTEK ALPHA V2.4 编译与烧录

更新：2026-10-07。硬件：i.MX6ULL、512MB DDR、8GB eMMC、ALPHA V2.4。上游说明见 [README](README)。

## 1. 已验证与设备编号

已验证从 SD 启动、DDR/串口、ENET2 网络、SD/eMMC 识别和 Linux 引导；用户已确认 U-Boot 烧录至 eMMC 验证成功，能保存 env。reset 修复已加入源码，尚缺上板成功反馈；断电后环境保留也应单独测试。

| 项目 | 编号/位置 |
|---|---|
| U-Boot SD | mmc0，约 14.5GiB |
| U-Boot eMMC | mmc1，约 7.3GiB |
| eMMC 用户区 | `mmc dev 1 0` |
| eMMC boot0 | `mmc dev 1 1`，独立 4MiB |
| eMMC FAT 软件分区 | `mmc 1:1`，用户区偏移 1MiB 起、32MiB |
| env | 启动设备用户区偏移 0xE0000（896KiB），大小 0x2000（8KiB） |

Linux 主机的 `/dev/sdX` 与 U-Boot mmc 编号不是同一套编号。每次插卡需重新检查。

## 2. Ubuntu 构建

源码：`/home/woody/alpha/woody/u-boot-2026.07`，Windows 挂载为 `W:\alpha\woody\u-boot-2026.07`。

```bash
sudo apt update
sudo apt install build-essential gcc-arm-linux-gnueabihf binutils-arm-linux-gnueabihf \
    bison flex libssl-dev libgnutls28-dev pkg-config python3 python3-dev \
    python3-setuptools python3-pyelftools swig device-tree-compiler
cd ~/alpha/woody/u-boot-2026.07
make O=out-alpha ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- alientek_alpha_imx6ull_defconfig
make O=out-alpha ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j"$(nproc)"
```

普通增量构建只需第二条 make；defconfig 改动后必须重新执行配置，已有自定义 out-alpha/.config 则先备份。缺少 gnutls/gnutls.h 是主机 mkeficapsule 依赖问题。

烧录产物为 **out-alpha/u-boot-dtb.imx**，已经包含启动头和 DTB，不要改用 u-boot.bin 或再次运行 imxdownload。

```bash
stat -c '%n: %s bytes' out-alpha/u-boot-dtb.imx
grep -E '^CONFIG_(ENV_OFFSET|ENV_SIZE|ENV_MMC_EMMC_HW_PARTITION|SUPPORT_EMMC_BOOT)=' out-alpha/.config
```

应为 ENV_OFFSET=0xE0000、ENV_SIZE=0x2000、ENV_MMC_EMMC_HW_PARTITION=0、SUPPORT_EMMC_BOOT=y。2026-10-04 镜像为 424960 字节（415KiB），以后以实际构建为准。

## 3. 烧录 SD：Ubuntu 操作

先识别读卡器，卸载其已挂载分区。历史上 `/dev/sda` 是 SD，系统盘为 NVMe，但不能永远照搬。旧环境曾因 `/dev/sdb` 是普通文件而出现虚假烧录成功。

```bash
lsblk -o NAME,TYPE,SIZE,MODEL,MOUNTPOINTS
sudo fdisk -l /dev/sda
ls -l /dev/sda
```

只有确认 /dev/sda 是目标 SD 块设备后，才执行下面脚本。已验证 SD 首分区起点为扇区8192（4MiB）；若布局不同需重新核对。脚本备份前4MiB；应事先按 lsblk 输出卸载目标分区。

```bash
(
set -eu
DEV=/dev/sda
IMAGE=out-alpha/u-boot-dtb.imx
test -b "$DEV" || { echo '停止：目标不是块设备'; exit 1; }
size=$(stat -c %s "$IMAGE")
blocks=$(( (size + 511) / 512 ))
# SD 镜像从1KiB开始，必须在896KiB的环境变量之前结束。
test "$blocks" -gt 0
test $((1024 + blocks * 512)) -le $((0xE0000))
sudo dd if="$DEV" of="sd-prefix-backup-$(date +%Y%m%d-%H%M%S).bin" bs=512 count=8192 status=progress
sudo dd if="$IMAGE" of="$DEV" bs=512 seek=2 conv=notrunc,fsync,nocreat status=progress
sudo dd if="$DEV" of=sd-uboot-readback.bin bs=512 skip=2 count="$blocks" status=progress
cmp -n "$size" "$IMAGE" sd-uboot-readback.bin
echo 'PASS: SD 回读与镜像一致'
sync
)
```

nocreat 属于 conv，不属于 oflag。确认成功后安全弹出 SD，断电设置 SD 启动拨码再上电。

## 4. 烧录 eMMC boot0：从 SD 启动 U-Boot 操作

本板已确认 eMMC 的 PARTITION_CONFIG 为 BOOT_ACK=1、BOOT_PARTITION_ENABLE=1（boot0）。保留可启动 SD 作为恢复入口。先核对：

```text
mmc dev 1 0
mmc info
mmc part
mmc partconf 1
```

下面步骤仅适用于上述已验证的 boot0 布局。用户区 FAT 无需重分区。将最新镜像复制到服务器实际 TFTP 根目录（按服务器配置确定，不能假定）：

```bash
TFTP_ROOT=/实际的TFTP根目录
sudo install -m 0644 out-alpha/u-boot-dtb.imx "$TFTP_ROOT/u-boot-dtb.imx"
```

开发板执行，确认 TFTP 成功后再计算扇区数：

```text
setenv ethaddr 00:04:9f:04:d2:35
setenv ipaddr 192.168.5.2
setenv serverip 192.168.5.226
tftpboot 80800000 u-boot-dtb.imx
setenv ubsize ${filesize}
setexpr ubblocks ${ubsize} + 1ff
setexpr ubblocks ${ubblocks} / 200
printenv ubsize ubblocks
```

U-Boot 这些参数按十六进制。boot0 为4MiB，必须满足 `2 + ubblocks <= 0x2000`。本次415KiB镜像对应 ubblocks=0x33e。

先备份整个旧 boot0 到 SD FAT；确认 SD 空间足够，已有同名备份时换名：

```text
mmc dev 1 1
mmc read 84000000 0 2000
fatwrite mmc 0:1 84000000 emmc-boot0-backup.bin 400000
```

仅在备份成功后写入。每条命令成功再继续，特别是 mmc dev 不能失败后仍继续写：

```text
mmc dev 1 1
mmc write 80800000 2 ${ubblocks}
mmc read 82000000 2 ${ubblocks}
cmp.b 80800000 82000000 ${ubsize}
mmc dev 1 0
mmc partconf 1
```

从 boot0 偏移1KiB写入；cmp.b 必须全部一致。非整扇区镜像最后会写入额外填充字节，比较按实际长度。此操作更新旧 boot0 U-Boot，不修改用户区 FAT。

本板已有正确启动配置，无需重新设置 partconf、bootbus 或执行一次性硬件分区操作。断电、设置 eMMC 启动拨码、再上电确认版本。拨码按实际板卡说明设置，本文不猜测拨码位序。

## 5. 环境变量与 Linux 启动

SD 启动 saveenv 写 SD，eMMC 启动 saveenv 写 eMMC；手动 mmc dev 切换不改变目标。env 都位于用户区896KiB，与 boot0 独立。不能使用旧的1MiB偏移，否则会覆盖 eMMC FAT 开头。

从目标介质启动后设置：

```text
setenv ethaddr 00:04:9f:04:d2:35
setenv ipaddr 192.168.5.2
setenv serverip 192.168.5.226
setenv bootcmd 'if tftpboot 80800000 zImage; then if tftpboot 83000000 imx6ull-alientek-alpha-v27-emmc.dtb; then bootz 80800000 - 83000000; fi; fi'
printenv bootargs
```

保留已验证的 NFS bootargs。若重新设置，必须使用 .226 的实际导出目录；旧目录不能默认照搬：

```text
setenv bootargs 'console=ttymxc0,115200 root=/dev/nfs nfsroot=192.168.5.226:/实际的NFS根目录,vers=3,proto=tcp rw ip=192.168.5.2:192.168.5.226:192.168.5.1:255.255.255.0::eth0:off'
saveenv
```

上面路径是占位符，替换后才能执行。先确认 saveenv 成功，再断电测试保留情况。run bootcmd 可立即测试启动。MAC 仅供本板使用，多板须保持唯一。

## 6. 板级实现与排错

- 板级代码：board/alientek/alpha_imx6ull；配置：configs/alientek_alpha_imx6ull_defconfig；设备树：arch/arm/dts/imx6ull-alientek-alpha-v24.dts。
- DDR 参数取自旧版实际工作镜像的 DCD，512MiB。
- ENET2 使用 PHY 地址1、GPIO5_IO08复位，低100ms、释放后等待200ms。地址0可读ID并不表示它属于ENET2。
- No valid MAC address：先设置ethaddr；本板设置后ping已成功。
- eMMC电压选择超时：检查GPIO4_IO10复位；当前代码低500us、释放后等待1ms，已有成功识别记录。
- reset原问题为链接到了空reset_cpu弱函数。当前启用SYSRESET和WDOG1内部软件复位，并关闭看门狗自动计时；仍待独立上板验证。
- mmc partconf只打印帮助：确认CONFIG_SUPPORT_EMMC_BOOT=y并重新构建烧录。
- 修改配置却无变化：核对out-alpha/.config、镜像时间、真实块设备及回读结果。

## 7. 验收与恢复

检查启动版本、DDR、mmc1信息、ping、Linux NFS启动、saveenv及断电保留、reset。eMMC启动失败时断电切回保留的SD，检查boot0和镜像；先定位问题再恢复备份，不执行整盘擦除或重新分区。