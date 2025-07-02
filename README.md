## FixDistanceCurveModifierInUE5.6

在UE5.6.0版本，DistanceCurveModifier功能异常，只会生成一条值全部为0的直线。<br>
等待官方解决可能还需要等较长时间。可以将此仓库文件夹里两个C++文件复制到你的项目中并编译。使用EastingDistanceCurve即可正确生成Distance曲线。
注：至少添加这3个模块依赖 AnimationModifiers   AnimationModifierLibrary   AnimationBlueprintLibrary