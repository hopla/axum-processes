/****************************************************************************
**
** Copyright (C) 2005-2006 Trolltech ASA. All rights reserved.
**
** This file is part of the example classes of the Qt Toolkit.
**
** This file may be used under the terms of the GNU General Public
** License version 2.0 as published by the Free Software Foundation
** and appearing in the file LICENSE.GPL included in the packaging of
** this file.  Please review the following information to ensure GNU
** General Public Licensing requirements will be met:
** http://www.trolltech.com/products/qt/opensource.html
**
** If you are unsure which license is appropriate for your use, please
** review the following information:
** http://www.trolltech.com/products/qt/licensing.html or contact the
** sales department at sales@trolltech.com.
**
** This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
** WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
**
****************************************************************************/

#include <QtGui>

#include "DNRWidget.h"
#include <private/qwidget_p.h>


class DNRWidgetPrivate : public QWidgetPrivate
{
    Q_DECLARE_PUBLIC(DNRWidget)
public:
	DNRWidgetPrivate()
	{}
};

DNRWidget::DNRWidget(QWidget *parent)
    : QWidget(*new DNRWidgetPrivate, parent, 0 )
{
   setWindowTitle(tr("DNR Widget"));
   resize(50, 50);
}

QObject *DNRWidget::connectionsObject()
{
	return this;
}

QObjectList DNRWidget::GetReceiverList(const char *signal)
{
	return d_func()->receiverList(signal);
}

QStringList DNRWidget::GetReceivingSlotsList(const char *signal, const char *receiver)
{
	return d_func()->receivingSlotsList(signal, receiver);
}

void DNRWidget::mousePressEvent(QMouseEvent *event)
{
	if (event->buttons() & Qt::RightButton)
   {
		QMenu menu(this);

		QMenu *ConnectMenu = menu.addMenu(tr("Connect")+ " - " + connectionsObject()->objectName());
		QMenu *DisconnectMenu = menu.addMenu(tr("Disconnect")+ " - " + connectionsObject()->objectName());

		int cntObjects = 0;

		//Connect menu...
		const QMetaObject *TempObject = connectionsObject()->metaObject();
 		for (int cntSenderMethods=TempObject->methodOffset(); cntSenderMethods<TempObject->methodCount(); cntSenderMethods++)
		{
			QMetaMethod SenderMetaMethod = TempObject->method(cntSenderMethods);

			if (SenderMetaMethod.methodType() == QMetaMethod::Signal)
			{
				QString SignalName = SenderMetaMethod.signature();
				QObjectList ObjectList = parent()->children();
				QMenu *SignalMenu = ConnectMenu->addMenu(SignalName.toAscii().constData());
				int cntReceivers = 0;

				QStringList ObjectStringList;
				for (int cntObject = 0; cntObject<ObjectList.count(); cntObject++)
				{
					ObjectStringList << ObjectList.at(cntObject)->objectName();
				}
				ObjectStringList.sort();

				for (int cntObject = 0; cntObject<ObjectStringList.count(); cntObject++)
				{
					QObject *ReceivingObject = parent()->findChild<QObject *>(ObjectStringList.at(cntObject));

					if (ReceivingObject != NULL)
					{
						QString ReceivingObjectName = ReceivingObject->objectName();

						QMenu *SlotMenu = SignalMenu->addMenu(ReceivingObjectName);
						int cntSlots = 0;

						for (int cntReceiverMethods=ReceivingObject->metaObject()->methodOffset(); cntReceiverMethods<ReceivingObject->metaObject()->methodCount(); cntReceiverMethods++)
						{
							QMetaMethod ReceiverMetaMethod = ReceivingObject->metaObject()->method(cntReceiverMethods);

							if (ReceiverMetaMethod.methodType() == QMetaMethod::Slot)
							{
								if (CheckConnectSignature(SignalName.toAscii().constData(), ReceiverMetaMethod.signature()))
								{
									QStringList TempStrings = GetReceivingSlotsList(SignalName.toAscii().constData(), ReceivingObjectName.toAscii().constData());
									bool AlreadyConnected = false;

									for (int cntReceivingSlots=0; cntReceivingSlots<TempStrings.count(); cntReceivingSlots++)
									{
										if (((QString)ReceiverMetaMethod.signature()) == TempStrings.at(cntReceivingSlots))
										{
											AlreadyConnected = true;
										}
									}

									if (!AlreadyConnected)
									{
										QAction *newAct = new QAction(ReceiverMetaMethod.signature(), this);

										QStringList ActionData;

										ActionData << SignalName;
										ActionData << ReceivingObjectName;
										ActionData << ReceiverMetaMethod.signature();

										newAct->setData(ActionData);

										connect(newAct, SIGNAL(triggered()), this, SLOT(PopupMenuConnectTrigger()));
										SlotMenu->addAction(newAct);

										cntSlots++;
									}
								}
							}
						}
						if  (cntSlots != 0)
						{
							cntReceivers++;
						}
						else
						{
							delete SlotMenu;
						}
					}
				}

				if (cntReceivers != 0)
				{
					cntObjects++;
				}
				else
				{
					delete SignalMenu;
				}

			}
		}
		if (cntObjects == 0)
		{
			delete ConnectMenu;
		}

		//Disconnect menu...
		int cntConnections = 0;
 		for (int cntMethods=TempObject->methodOffset(); cntMethods<TempObject->methodCount(); cntMethods++)
		{
			QMetaMethod MetaMethod = TempObject->method(cntMethods);

			if (MetaMethod.methodType() == QMetaMethod::Signal)
			{
				QString SignalName = MetaMethod.signature();
				QObjectList ObjectList = GetReceiverList(SignalName.toAscii().constData());

				if (ObjectList.count())
				{
					QMenu *SignalMenu = DisconnectMenu->addMenu(SignalName.toAscii().constData());

					QStringList ObjectStringList;
					for (int cntObject = 0; cntObject<ObjectList.count(); cntObject++)
					{
						ObjectStringList << ObjectList.at(cntObject)->objectName();
					}
					ObjectStringList.sort();

					for (int cntObject = 0; cntObject<ObjectStringList.count(); cntObject++)
					{
						QObject *ReceivingObject = parent()->findChild<QObject *>(ObjectStringList.at(cntObject));

						if (ReceivingObject != NULL)
						{
							QString ReceivingObjectName = ReceivingObject->objectName();
							QMenu *SlotMenu = SignalMenu->addMenu(ReceivingObjectName);

							QStringList TempStrings = GetReceivingSlotsList(SignalName.toAscii().constData(), ReceivingObjectName.toAscii().constData());
							for (int cntString=0; cntString<TempStrings.count(); cntString++)
							{
								QAction *newAct = new QAction(TempStrings.at(cntString), this);

								QStringList ActionData;

								ActionData << SignalName;
								ActionData << ReceivingObjectName;
								ActionData << TempStrings.at(cntString);

								newAct->setData(ActionData);

								connect(newAct, SIGNAL(triggered()), this, SLOT(PopupMenuDisconnectTrigger()));
								SlotMenu->addAction(newAct);

								cntConnections++;
							}
						}
					}
				}
			}
		}
		if (cntConnections == 0)
		{
			delete DisconnectMenu;
		}


		if ((cntObjects != 0) || (cntConnections != 0))
		{
      	menu.exec(event->globalPos());
		}
   }
}

void DNRWidget::PopupMenuConnectTrigger()
{
	QString SignalName="";
	QString SlotName="";

	SignalName.setNum(QSIGNAL_CODE);
	SlotName.setNum(QSLOT_CODE);

	QAction *MenuAction = (QAction *)sender();
	QStringList ActionData = MenuAction->data().toStringList();
	QObject *ReceivingObject = parent()->findChild<QObject *>(ActionData.at(1));

	SignalName.setNum(QSIGNAL_CODE);
	SignalName += ActionData.at(0);
	SlotName.setNum(QSLOT_CODE);
	SlotName += ActionData.at(2);

	connect(connectionsObject(), SignalName.toAscii().constData(), ReceivingObject, SlotName.toAscii().constData()/*, Qt::DirectConnection*/);

	update();
}


void DNRWidget::PopupMenuDisconnectTrigger()
{
	QString SignalName="";
	QString SlotName="";

	SignalName.setNum(QSIGNAL_CODE);
	SlotName.setNum(QSLOT_CODE);

	QAction *MenuAction = (QAction *)sender();
	QStringList ActionData = MenuAction->data().toStringList();
	QObject *ReceivingObject = parent()->findChild<QObject *>(ActionData.at(1));

	SignalName.setNum(QSIGNAL_CODE);
	SignalName += ActionData.at(0);
	SlotName.setNum(QSLOT_CODE);
	SlotName += ActionData.at(2);

	disconnect(connectionsObject(), SignalName.toAscii().constData(), ReceivingObject, SlotName.toAscii().constData());

	update();
}

bool DNRWidget::CheckConnectSignature(const char *signal, const char *method)
{
    const char *s1 = signal;
    const char *s2 = method;
    while (*s1++ != '(') { }        // scan to first '('
    while (*s2++ != '(') { }
    if (qstrcmp(s1,s2) == 0)        // method has no args or
        return true;           		//   exact match

    return false;
}

