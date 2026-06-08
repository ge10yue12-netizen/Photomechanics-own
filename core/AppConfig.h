#pragma once



// AppConfig — 每次启动将界面恢复为固定默认值（不读写 QSettings，避免遗留上一会话配置）



namespace Ui

{

class QtProject_1Class;

}



class AppConfig

{

public:

    static void loadUi(Ui::QtProject_1Class &ui); // 启动：全部控件恢复默认

    static void saveUi(const Ui::QtProject_1Class &ui); // 保留空实现，兼容旧调用点

};


