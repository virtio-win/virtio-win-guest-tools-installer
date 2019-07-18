#!/usr/bin/python -u

from constants import * as const
import datetime
import getopt
import ovirtsdk.api
import ovirtsdk.xml
import sys
import time
import urllib


def returnName(vm, vmType):
    return vm + '-' + vmType

def createVM(vmName, templateName, cluster = None, serial = None):
    global hostID
    if cluster:
        cluster = api.clusters.get(name=cluster)
    else:
        cluster = api.clusters.get(name='Default')
    template = api.templates.get(name=templateName)
    if serial:
        serialParam = ovirtsdk.xml.params.SerialNumber()
        serialParam.value = serial
        param = ovirtsdk.xml.params.VM(name=vmName, cluster=cluster, template=template,serial_number=serialParam)
    else :
        param = ovirtsdk.xml.params.VM(name=vmName, cluster=cluster, template=template)
    print "Creating VM %s from template %s..." % (vmName, templateName)
    try:
        vmHndl = api.vms.add(param)
    except Exception as e:
        print "Failed to create VM (%s) - %s" % (vmName, str(e))
        return False
    if not hostID:
	hostID = api.vms.get(name=vmName).id
	print "hostID is set to %s." % hostID
    while api.vms.get(name=vmName).status.state != 'down':
        time.sleep(1)
    print "VM %s created." % vmName
    return True

def startVM(vmName):
    print "Starting VM %s..." % vmName
    try:
        api.vms.get(name=vmName).start()
        print "Waiting for VM %s to reach Up status" % vmName
        while api.vms.get(name=vmName).status.state != 'up':
            time.sleep(1)
        return True
    except Exception as e:
        print "Failed to start VM (%s) - %s" % (vmName, str(e))
        return False

def connectISO(vmName, isoName):
    print "Connecting ISO %s to %s..." % (isoName, vmName)
    try:
        vm = api.vms.get(name=vmName)
        cdrom = vm.cdroms.get(id="00000000-0000-0000-0000-000000000000")
        isofile = ovirtsdk.xml.params.File(id=isoName)
        cdrom.set_file(isofile)
        cdrom.update(current=True)
        return True
    except Exception as e:
        print "Failed to attach ISO %s to %s - %s" % (isoName, vmName, str(e))
        return False

def resetVMRecord(hostID):
    sys.stdout.write("Resetting VM record... ")
    request = urllib.urlopen(url.format(action='reset', hostID=hostID))
    status = request.getcode()
    if status == 202:
        print "OK"
        return True
    elif status == 409:
        print "\r\nError resetting VM record."
        return False

def getErrorCode():
    print "Quering for WGT installation status..."
    begin = time.time()
    while True:
        request = urllib.urlopen(url.format(action='query', hostID=hostID))
        status = request.getcode()
        if status == 200:
            break
        elif status == 404:
            print "    Still waiting for results..."
            time.sleep(30)
        else:
            print "Got invalid status - %i" % status
            break
    end = time.time()
    sec = int(end - begin)
    d = datetime.datetime(1,1,1) + datetime.timedelta(seconds=sec)
    print "Waited for the result for %d hour(s), %d minute(s) and %d second(s)." % (d.hour, d.minute, d.second)
    errorCode = request.read()
    if int(errorCode) == 3010:
        print "RHEV Tools installation completed successfully."
        return True
    else:
        print "RHEV Tools installation completed with errorCode of %s." % errorCode
        return False

def getGuestInfo(vmName):
    print "Quering for Guest Agent information..."
    for i in range(0,11):
        try:
            if api.vms.get(name=vmName).statistics.get(name="memory.used").get_values().get_value()[0].get_datum() == 0.0:
                print "    No memory usage data is available..."
                raise Exception("Guest Agent doesn't seem to be running.")
            gi = api.vms.get(name=vmName).get_guest_info()
            if gi:
                giStr =  "Guest information is :\r\n"
                for ip in gi.ips.ip:
                    giStr += "    ip : %s\r\n" % ip.address
                giStr += "    fqdn : %s" % gi.fqdn.split(',')[0]
                print giStr
                return True
        except Exception as e:
            print "    Still waiting for Guest information..."
            time.sleep(30)
    print "Guest info is not available."
    return False

def poweroffVM(vmName):
    print "Powering off VM %s..." % vmName
    try:
        if api.vms.get(name=vmName).status.state != 'down':
            api.vms.get(name=vmName).stop()

            while api.vms.get(name=vmName).status.state != 'down':
                time.sleep(1)
        else:
            print "VM (%s) already down." % vmName

    except Exception as e:
        print "Failed to poweroff VM (%s) - %s" % (vmName, str(e))

def deleteVM(vmName):
    print "Removing VM %s..." % vmName
    try:
        api.vms.get(name=vmName).delete()
        print "Waiting for VM %s to be deleted..." % vmName
        while vmName in [vm.name for vm in api.vms.list()]:
            time.sleep(1)
    except Exception as e:
        print "Failed to delete VM (%s) - %s." % (vmName, str(e))

api = ovirtsdk.api.API(
      url='https://{fqdn}:{port}/api'.format(
              fqdn=fqdn,
              port=port,
            ),
      username=user,
      password=passwd,
      ca_file='/var/lib/jenkins/jobs/ca.crt',
)

def usage():
    print 'runwgtcijob.py --vmname=<VM Name> --template=<VM Template> --isoname=<isoName> --hostid=<GUID>'

try:
    opts, args = getopt.getopt(sys.argv[1:],'c:hi:n:t:u:w:',['vmname=', 'template=', 'hostid=', 'isoname=', 'nodestroy', 'keeprecord', 'serial=', 'wait=', 'cluster='])
except getopt.GetoptError:
    usage()    
    sys.exit(1)

vmName = None
template = None
isoNames = None
hostID = None
destroyVM = True
keepRecord = False
serial = None
wait = 120
cluster = None

for opt, arg in opts:
    if opt in ('-h', '--help'):
        usage()
        sys.exit()
    elif opt in ('-n', '--vmname'):
        vmName = arg
    elif opt in ('-t', '--template'):
        template = arg
    elif opt in ('-i', '--isoname'):
        isoNames = arg
    elif opt in ('-u', '--hostid'):
        hostID = arg
    elif opt in ('-s', '--serial'):
        serial = arg
    elif opt in ('--nodestroy'):
        destroyVM = False
    elif opt in ('--keeprecord'):
        keepRecord = True
    elif opt in ('-w', '--wait'):
        wait = int(arg)
    elif opt in ('-c', '--cluster'):
        cluster = arg

if not vmName or not template or not isoNames:
    print 'Not enough information supplied'
    usage()
    sys.exit(1)

if api.vms.get(vmName) and api.vms.get(vmName).status.state == 'up':
    print "Error - VM (%s) is already running." % vmName
    sys.exit(1)
else:
    if not createVM(vmName, template, cluster, serial) or not startVM(vmName):
        sys.exit(1)

    print ""

    isoNames = (iso for iso in isoNames.split(',') if iso)

    for isoName in isoNames:
        if not resetVMRecord(hostID) or not connectISO(vmName, isoName) or not getErrorCode():
            sys.exit(1)
        print "Waiting for %i seconds for the VM to restart... " % wait
        time.sleep(wait)
        if not getGuestInfo(vmName):
            sys.exit(1)
        print ""

    if api.vms.get(vmName).status.state == 'up' and destroyVM:
        print "VM (%s) is running, powering off..." % vmName
        poweroffVM(vmName)
        deleteVM(vmName)

    if not keepRecord:
        resetVMRecord(hostID)
