#include "PrintOutputData.h"

PrintOutputData PrintOutputData::createDefault()
{
    PrintOutputData data;
    auto &r = data.rows;

    r.append({QString(), QString(), QString(), QString(), QString(), QString(), true});
    r.last().leftName = QStringLiteral("产品基本信息");

    auto row = [&](const QString &ln, const QString &lv, const QString &lu,
                   const QString &rn, const QString &rv, const QString &ru) {
        r.append({ln, lv, lu, rn, rv, ru, false});
    };

    row("产品图号", "TY442-1", "", "低压扁线线规尺寸", "标准尺寸", "");
    row("绕组形式", "双绕组", "", "高压扁线线规尺寸", "标准尺寸", "");
    row("铁芯结构形式", "类椭圆形", "", "高压漆包线线规尺寸", "标准尺寸", "");
    row("低压线圈结构形式", "箔绕", "", "高压层间绝缘布置", "全绝缘", "");
    row("高压线圈结构形式", "多层圆筒式", "", "冷却装置", "片式散热器", "");
    row("产品型号", "S13配变", "", "变压器油", "25#", "");
    row("联结组别", "Dyn", "", "是否带储油柜", "是", "");
    row("频率", "50", "Hz", "是否采用压包方案", "否", "");
    row("硅钢片牌号", "V23SQGD085", "", "是否采用真空注油", "是", "");
    row("铁轭结构形式", "D形扼", "", "油与箱盖是否接触", "是", "");
    row("低压线圈材质", "铜", "", "高压线圈段间电压", "全电压", "");
    row("高压线圈材质", "铜", "", "高压线圈分接布置", "一段外侧", "");
    row("低压电磁线类型", "铜箔", "", "环境等级", "C3", "");
    row("高压电磁线类型", "纸包扁铜线", "", "", "", "");

    r.append({QString(), QString(), QString(), QString(), QString(), QString(), true});
    r.last().leftName = QStringLiteral("一 技术条件");

    row("容量", "1600", "kVA", "负载损耗设计允许偏差", "0", "%");
    row("高压电压等级", "10", "kV", "总损耗标准值", "15670", "W");
    row("高压额定电压", "6300", "V", "总损耗设计允许偏差", "0", "%");
    row("高压调压级数", "2", "个", "短路阻抗标准值", "4.5", "%");
    row("高压调压级电压", "2.5", "%", "短路阻抗最小允许偏差", "-10", "%");
    row("低压额定电压", "0.8", "kV", "短路阻抗最大允许偏差", "10", "%");
    row("空载损耗标准值", "1170", "W", "最高环境温度", "40", "℃");
    row("空载损耗设计允许偏差", "0", "%", "最高海拔高度", "1000", "m");
    row("空载电流标准值", "0.4", "%", "油顶层温升限值", "60", "K");
    row("空载电流设计允许偏差", "30", "%", "低压线圈对油温升限值", "65", "K");
    row("负载损耗标准值", "14500", "W", "高压线圈对油温升限值", "65", "K");

    r.append({QString(), QString(), QString(), QString(), QString(), QString(), true});
    r.last().leftName = QStringLiteral("二 线圈电气参数");

    row("高压额定线电压", "6300", "V", "高压最小分接线电压", "5985", "V");
    row("高压额定相电压", "6300", "V", "高压最小分接相电压", "5985", "V");
    row("高压额定线电流", "146.629", "A", "高压最小分接相电流", "89.112", "A");
    row("高压额定相电流", "84.656", "A", "低压额定线电压", "800", "V");
    row("高压最大分接线电压", "6615", "V", "低压额定相电压", "461.88", "V");
    row("高压最大分接相电压", "6615", "V", "低压额定线电流", "1154.701", "A");
    row("高压最大分接相电流", "80.625", "A", "低压额定相电流", "1154.701", "A");

    r.append({QString(), QString(), QString(), QString(), QString(), QString(), true});
    r.last().leftName = QStringLiteral("三 铁芯");

    row("铁芯直径(铁芯短轴)", "245", "mm", "铁芯净截面", "657.1388", "cm2");
    row("铁芯长轴", "368", "mm", "铁芯磁通密度", "1.439", "T");

    return data;
}
