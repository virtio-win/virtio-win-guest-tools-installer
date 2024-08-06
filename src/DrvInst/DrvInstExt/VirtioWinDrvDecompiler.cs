namespace DrvInstExt
{
    using System;
    using System.Collections;
    using System.Reflection;
    using System.Xml;
    using System.Xml.Schema;
    using Microsoft.Tools.WindowsInstallerXml;

    using VirtioWinDrvInstall = DrvInstExt;
    //Microsoft.Tools.WindowsInstallerXml.Extensions.VirtioWinDrvInstall;

    using Wix = Microsoft.Tools.WindowsInstallerXml.Serialize;
    public sealed class DrvInstallerDecompiler : DecompilerExtension
    {
        public override void DecompileTable(Table table)
        {
            switch (table.Name)
            {
                case "DriverPackages":
                    this.DriverPackages(table);
                    break;
                default:
                    base.DecompileTable(table);
                    break;
            }
        }
        private void DriverPackages(Table table)
        {
            foreach (Row row in table.Rows)
            {
                Driver driver = new DrvInstExt.Driver();

                int attributes = (int)row[1];
                if (0x1 == (attributes & 0x1))
                {
                    driver.ForceInstall = VirtioWinDrvInstall.YesNoType.yes;
                }

                if (0x2 == (attributes & 0x2))
                {
                    driver.PlugAndPlayPrompt = VirtioWinDrvInstall.YesNoType.no;
                }

                if (0x4 == (attributes & 0x4))
                {
                    driver.AddRemovePrograms = VirtioWinDrvInstall.YesNoType.no;
                }

                if (0x8 == (attributes & 0x8))
                {
                    driver.Legacy = VirtioWinDrvInstall.YesNoType.yes;
                }

                if (0x10 == (attributes & 0x10))
                {
                    driver.DeleteFiles = VirtioWinDrvInstall.YesNoType.yes;
                }

                if (null != row[2])
                {
                    driver.Sequence = (int)row[2];
                }

                Wix.Component component = (Wix.Component)this.Core.GetIndexedElement("Component", (string)row[0]);
                if (null != component)
                {
                    component.AddChild(driver);
                }
                else
                {
                    this.Core.OnMessage(WixWarnings.ExpectedForeignRow(row.SourceLineNumbers, table.Name, row.GetPrimaryKey(DecompilerCore.PrimaryKeyDelimiter), "Component", (string)row[0], "Component"));
                }
            }
        }
    }
}