# 音频
## AVChannelLayout
音频采样通道
### 参数
* order: 通道排列顺序，默认为解码得到的`AV_CHANNEL_ORDER_NATIVE`,一般不用变更，但也可以自定义通道顺序`AV_CHANNEL_ORDER_CUSTOM`, 然后设置AvChanneLayout.map映射即可。
* .m.mask: 通道掩码，描述音频的通道布局，由一组特定的通道标志组成，每个标志对应一种通道（如左通道，右通道）

通道布局掩码的常用值：


| 标志 | 掩码 | 描述 |
| --- | ---| ---|
| AV_CH_LAYOUT_MONO |	0x00000001 |	单声道（Mono）|
AV_CH_LAYOUT_STEREO	| 0x00000003	| 立体声（Stereo） |
AV_CH_LAYOUT_2POINT1 |	0x00000007 |	2.1 声道|
AV_CH_LAYOUT_5POINT1 |	0x0000003F |	5.1 环绕声 |
AV_CH_LAYOUT_7POINT1 |	0x000000FF |	7.1 环绕声 |

每个通道标志和掩码进行与运算即可求出相应通道值。


