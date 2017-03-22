#include "jiraagent.h"

#include "branchitem.h"
#include "mainwindow.h"
#include "vymmodel.h"

#include <QHash>

extern Main *mainWindow;
extern QDir vymBaseDir;
extern bool debug;


JiraAgent::JiraAgent (BranchItem *bi,const QString &u)
{
    if (!bi) 
    {
	qWarning ("Const JiraAgent: bi == NULL");
	delete (this);
	return;
    }
    branchID=bi->getID();
    VymModel *model = bi->getModel();
    modelID = model->getModelID();

    //qDebug()<<"Constr. JiraAgent for "<<branchID;

    url = u;

    QStringList args;

    if (url.contains("/browse/")) 
    {
	missionType = SingleTicket;
	QRegExp rx("browse/(.*)$");
        rx.setMinimal(true);
	if (rx.indexIn(url) != -1)
	{
	    ticketID = rx.cap(1);
	    args << ticketID;
            qDebug() << "JiraAgent:  ticket id: "<< ticketID;
	} else
	{
	    qWarning() << "JiraAgent: No ticketID found in: " << url;
	    delete (this);
	    return;
	}

    } else if (u.contains("buglist.cgi"))   // FIXME-0
    {
	missionType = Query; //FIXME-0 query not supported yet by new bugger
	args << "--query";
	args << url;
    } else
    {
	qDebug() << "JiraAgent: Unknown command:\n" << url; // FIXME-0
	delete (this);
	return;
    }
	

    ticketScript = vymBaseDir.path() + "/scripts/jigger";

    p = new VymProcess;

    connect (p, SIGNAL (finished(int, QProcess::ExitStatus) ), 
	this, SLOT (processFinished(int, QProcess::ExitStatus) ));

    p->start (ticketScript, args);
    if (!p->waitForStarted())
    {
	qWarning() << "JiraAgent::getJiraData couldn't start " << ticketScript;
	return;
    }	 

    // Visual hint that we are doing something  // FIXME-4 show spinner instead?
    if (missionType == SingleTicket)
        model->setHeading ("Updating: " + bi->getHeadingPlain(), bi ); //FIXME-4 translation needed?
	
}

JiraAgent::~JiraAgent ()
{
    //qDebug()<<"Destr. JiraAgent for "<<branchID;
    delete p;
}

void JiraAgent::processFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    if (exitStatus == QProcess::NormalExit)
    {
	result=p->getStdout().split("\n");
	QString err = p->getErrout();
	if (!err.isEmpty())
	    qWarning() << "JiraAgent Error: " << err;
	else 
	    processJiraData ();

    } else	
	qWarning() << "JiraAgent: Process finished with exitCode=" << exitCode;
    deleteLater();
}


void JiraAgent::processJiraData()
{
    // Find model from which we had been started
    VymModel *model = mainWindow->getModel (modelID);
    if (model)
    {
	// and find branch which triggered this mission
	BranchItem *missionBI = (BranchItem*)(model->findID (branchID));	    
	if (missionBI)
	{
	    // Here we go...

	    QRegExp re("(.*):(\\S*):\"(.*)\"");
	    re.setMinimal(false);
	    ticket_desc.clear();
	    ticket_prio.clear();
	    ticket_status.clear();

	    QStringList tickets; 
	    foreach (QString line,result)
	    {
		if (debug) qDebug() << "JiraAgent::processJiraData  line=" << line;
		if (re.indexIn(line) != -1) 
		{
		    if (re.cap(2) == "short_desc") 
		    {
			tickets.append(re.cap(1));
			ticket_desc[re.cap(1)] = re.cap(3).replace("\\\"","\"");
                        qDebug() << "shortdesc: " << re.cap(3);
		    }	
		    else if (re.cap(2) == "priority") 
                    {
			ticket_prio[re.cap(1)] = re.cap(3).replace("\\\"","\"");
                        qDebug() << "priority : " << re.cap(3);
                    }
		    else if (re.cap(2) == "ticket_status") 
                    {
			ticket_status[re.cap(1)] = re.cap(3).replace("\\\"","\"");
                        qDebug() << "status   : " << re.cap(3);
                    }
		}
	    }
	    if (ticket_desc.count() <= 0 )
		qWarning() << "JiraAgent: Couldn't find data";
	    else if (missionType == SingleTicket)
	    {
		// Only single ticket changed
		QString b = tickets.first();
		setModelJiraData (model, missionBI, b);
	    } else
	    {
		// Process results of query
		BranchItem *newbi;
		foreach (QString b,tickets)
		{
		    //qDeticket ()<<" -> "<<b<<" "<<ticket_desc[b];
		    newbi = model->addNewBranch(missionBI);    
		    newbi->setURL ("https://bugzilla.novell.com/show_bug.cgi?id=" + b); // FIXME-0
		    if (!newbi)
			qWarning() << "JiraAgent: Couldn't create new branch?!";
		    else
			setModelJiraData (model, newbi,b);
		}
	    } 
	} else
	    qWarning () << "JiraAgent: Found model, but not branch #" << branchID;
    } else
	qWarning () << "JiraAgent: Couldn't find model #" << modelID;


}

void JiraAgent::setModelJiraData (VymModel *model, BranchItem *bi, const QString &bugID)
{
    if (debug)
    {
        qDebug() << "JiraAgent::setModelJiraData for " << bugID;
    }

    QString ps = ticket_prio[ticketID];
    QStringList solvedStates;
    solvedStates << "Open";
    solvedStates << "To Do";
    solvedStates << "In Analysis";
    solvedStates << "Prepared";
    solvedStates << "Implemented";
    solvedStates << "In Overall Integration";	
    solvedStates << "Overall Integration Done";
    if (solvedStates.contains( ticket_status[ticketID] ) )
    {
        model->setHeadingPlainText ("(" + ps + ") - " + ticketID + " - " + ticket_desc[ticketID], bi);
    } else   
    {
        model->setHeadingPlainText (ps + " - " + ticketID + " - " + ticket_desc[ticketID], bi);
	model->colorSubtree (Qt::blue, bi);
    }
}
