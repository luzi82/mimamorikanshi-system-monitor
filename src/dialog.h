/*
 * MimamoriKanshi System Monitor - Dialog Header
 *
 * Settings dialog for configuring the plugin via Xfconf.
 */

#ifndef MIMAMORIKANSHI_DIALOG_H
#define MIMAMORIKANSHI_DIALOG_H

/* Forward declaration */
typedef struct _MimamorikanshiPlugin MimamorikanshiPlugin;

/* Show the settings dialog.  Called from the "configure-plugin" signal. */
void mimamorikanshi_dialog_show(MimamorikanshiPlugin *mmk);

#endif /* MIMAMORIKANSHI_DIALOG_H */
