#include "editor.hpp"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("ParadigmOriginScoreEditor"));
    QApplication::setOrganizationName(QStringLiteral("Community"));
    EditorWindow window;
    window.show();
    return application.exec();
}
