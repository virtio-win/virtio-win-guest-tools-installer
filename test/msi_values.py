
def get_expected_componentids():
    expected_values_x64 = {
        'CMP_WinServer2016_balloon':['{D604612F-EFD0-44CE-956C-5AD25518A742}'],
        'CMP_blnsvr_2k16_amd64':['{AC3A4978-2B0A-4E5C-9DAA-327C503E98C6}'],
        'CMP_WinServer2019_balloon':['{0279689F-2B56-47BB-BE08-2D66A0B9F4FC}'],
        'CMP_blnsvr_2k19_amd64':['{367F3AA4-7747-4FDF-A3F9-3D2766D9D07E}'],
        'CMP_WinServer2022_balloon':['{824217E9-568A-403A-82E1-408171BDB05B}'],
        'CMP_blnsvr_2k22_amd64':['{D87C64B1-1769-4D6D-B912-C7389AA5F997}'],
        'CMP_WinServer2025_balloon':['{9F8840A9-C234-40B4-BD08-3EC555C11A46}'],
        'CMP_blnsvr_2k25_amd64':['{9981155B-32C7-4BE0-80C0-C532C0F5045E}'],
        'CMP_Win10x64_balloon':['{75F1C56D-21E9-4A46-AF12-912C9ECD29D2}'],
        'CMP_blnsvr_w10_amd64':['{66B082AC-9CF4-44D1-9CE1-63F7235F6C1C}'],
        'CMP_Win11x64_balloon':['{7E770B97-A137-4350-837D-A562CEB878AC}'],
        'CMP_blnsvr_w11_amd64':['{1555D369-5A0F-4ED7-BF58-5B863398A00F}'],
        'CMP_WinServer2016_netkvm':['{F76DFEFF-FC33-45E8-876B-0013852A0BC4}'],
        'CMP_netkvm_netkvmp_2k16_amd64':['{FC1941E3-4364-4F12-B12B-5774DE07C875}'],
        'CMP_netkvm_netkvmco_2k16_amd64':['{6A71DDEB-0CF4-4A64-94F4-CE3F5F396CEF}'],
        'CMP_netkvm_readme_2k16_amd64':['{9310CAD4-1935-4C02-8AE4-F3C91BD9BFA1}'],
        'CMP_WinServer2019_netkvm':['{EDDE3DE9-AEBB-49D5-A232-F2B7310A60A8}'],
        'CMP_netkvm_netkvmp_2k19_amd64':['{20756063-1602-411E-B2D1-D7EE8945FC92}'],
        'CMP_netkvm_netkvmco_2k19_amd64':['{27B38D8F-0B8B-4261-8E26-5ACDA7BC5B18}'],
        'CMP_netkvm_readme_2k19_amd64':['{3D8E57A6-4643-4D57-BCC9-658696DB7F56}'],
        'CMP_WinServer2022_netkvm':['{3FC5E6F9-544A-49A0-963A-8452CB20E949}'],
        'CMP_netkvm_netkvmp_2k22_amd64':['{F68CD470-8A87-44FD-A9B7-A919DBE4F77B}'],
        'CMP_netkvm_netkvmco_2k22_amd64':['{F3A70552-1D26-4B75-A5FF-53921B0F0DBC}'],
        'CMP_netkvm_readme_2k22_amd64':['{3B7BE477-98FC-4E97-80EE-6EC875A49D5E}'],
        'CMP_WinServer2025_netkvm':['{84DCAC14-A463-4C15-9D7A-C30AE2B85419}'],
        'CMP_netkvm_netkvmp_2k25_amd64':['{CFEDAEE4-28AC-4C93-BA5E-B908694D17F6}'],
        'CMP_netkvm_netkvmco_2k25_amd64':['{E2BE4AFF-65D5-4211-9C5A-415B253B071A}'],
        'CMP_netkvm_readme_2k25_amd64':['{E49C31D3-07D8-4EAC-9621-79461827BBF0}'],
        'CMP_Win10x64_netkvm':['{DC8B83A8-6501-4494-81A7-454F43EB07E0}'],
        'CMP_netkvm_netkvmp_w10_amd64':['{9B862A39-90A4-4B10-84EC-B77CC83B2CB1}'],
        'CMP_netkvm_netkvmco_w10_amd64':['{2EC241D0-DFB0-4F2D-9E1F-F98475940BA4}'],
        'CMP_netkvm_readme_w10_amd64':['{85EDD336-0051-4F6B-89C3-5AD153E7B554}'],
        'CMP_Win11x64_netkvm':['{B931E603-768A-4FCE-A505-3E1435EBA378}'],
        'CMP_netkvm_netkvmp_w11_amd64':['{879DF7E6-B0BE-43B0-ADB4-02C916C2E6B1}'],
        'CMP_netkvm_netkvmco_w11_amd64':['{21928595-F2F6-4669-94C3-2E33BABC247D}'],
        'CMP_netkvm_readme_w11_amd64':['{20298575-7E69-47A6-82C9-7A9DEB802DB5}'],
        'CMP_WinServer2016_viorng':['{106DD383-EA41-4CED-A1DE-BAD4B1421B42}'],
        'CMP_viorng_viorngum_2k16_amd64':['{903768B1-286A-4B00-B3B7-B3704394D5D7}'],
        'CMP_WinServer2019_viorng':['{EB4404C4-34A2-4378-9D66-A4D8841BAE56}'],
        'CMP_viorng_viorngum_2k19_amd64':['{8D85AA97-4238-4A28-9A79-B009FBEA7961}'],
        'CMP_WinServer2022_viorng':['{8B71783C-4721-4203-BA5B-69664BEECCCD}'],
        'CMP_viorng_viorngum_2k22_amd64':['{6A477A10-5405-4EDE-B0BB-F5831DE6D644}'],
        'CMP_WinServer2025_viorng':['{8CAA9201-E14E-4E7D-A5A7-8F7CF51DB7AC}'],
        'CMP_viorng_viorngum_2k25_amd64':['{85BB4D3E-638A-4C04-A055-8A67EDEB43B0}'],
        'CMP_Win10x64_viorng':['{8600A950-7C8B-4EAE-84A4-8FFEA8BE906F}'],
        'CMP_viorng_viorngum_w10_amd64':['{32659F53-B2EB-4D72-A3D9-CBA7C6401D08}'],
        'CMP_Win11x64_viorng':['{09A012A4-0DA0-426E-95B8-5E91CC1AA174}'],
        'CMP_viorng_viorngum_w11_amd64':['{04DFD7AE-0E1D-4FC5-8F79-EC991DAF03E2}'],
        'CMP_WinServer2016_vioscsi':['{2581A672-FDCB-45E4-9C3E-2D143FAD9D88}'],
        'CMP_WinServer2019_vioscsi':['{9B505BFA-24F1-4AFA-8F20-1150AD7DC384}'],
        'CMP_WinServer2022_vioscsi':['{CD88A70E-5F84-4A11-BA3C-AD8D158F8D68}'],
        'CMP_WinServer2025_vioscsi':['{DE6561FC-13E4-4069-BC62-5B12455B4525}'],
        'CMP_Win10x64_vioscsi':['{D78BF955-D5B6-4080-A702-E98754D9174A}'],
        'CMP_Win11x64_vioscsi':['{4F28D54F-7DA0-4027-A61B-50355010345C}'],
        'CMP_WinServer2016_vioser':['{3DCC592D-542C-49E1-90EA-45177C01BC26}'],
        'CMP_WinServer2019_vioser':['{AA85E501-4E99-42A1-A519-9769D8C6ACAC}'],
        'CMP_WinServer2022_vioser':['{6AEBEAD8-7BE9-4981-A72F-14DC6E076D18}'],
        'CMP_WinServer2025_vioser':['{56F59B71-4B86-490F-BD6E-E7923FA59C33}'],
        'CMP_Win10x64_vioser':['{98A0368C-E2D6-49A8-9B8C-1E3DFBCFFD19}'],
        'CMP_Win11x64_vioser':['{91CBFAC6-D648-4330-A205-C44B571954CE}'],
        'CMP_WinServer2016_viostor':['{6AB4DF0A-AEC8-4F66-B9FE-AB47ADDCB857}'],
        'CMP_WinServer2019_viostor':['{57F26A76-3048-43AD-8852-412BF36D017B}'],
        'CMP_WinServer2022_viostor':['{83B88AE8-CE82-423F-9ED2-5403381105FE}'],
        'CMP_WinServer2025_viostor':['{E0D5CAFF-3EFA-4133-83C8-4EA3AF0160B6}'],
        'CMP_Win10x64_viostor':['{884ADCD2-0334-45AB-9806-F21ED4D16D48}'],
        'CMP_Win11x64_viostor':['{70FFAB18-7DAA-47D7-86B4-CEA358DEFB0C}'],
        'CMP_WinServer2016_viofs':['{3D1814BB-CDF2-4918-BD34-EA697A14D63B}'],
        'CMP_virtiofs_2k16_amd64':['{7F51A186-28F4-4C7C-986B-279AC73F6C9C}'],
        'CMP_WinServer2019_viofs':['{794FE5BB-E999-4257-A4C1-2E4E9DC4A213}'],
        'CMP_virtiofs_2k19_amd64':['{1D146EAB-CC13-45E8-A0BC-FA70A2F913AF}'],
        'CMP_WinServer2022_viofs':['{0EE9BBA1-9168-4D30-83BC-9DCD408BEB19}'],
        'CMP_virtiofs_2k22_amd64':['{E355531E-4609-4CD3-8F9C-361795825A02}'],
        'CMP_WinServer2025_viofs':['{A9C434B6-1D37-4DCE-B3D7-42C9299388A0}'],
        'CMP_virtiofs_2k25_amd64':['{0F21C2AA-95EE-49CA-9736-AAC12D9D4928}'],
        'CMP_Win10x64_viofs':['{E4F42A9F-D576-4606-B2BF-96D24E53E365}'],
        'CMP_virtiofs_w10_amd64':['{3D0B70D1-C98E-4986-A0D1-6CF4B1E06CC9}'],
        'CMP_Win11x64_viofs':['{78836E98-4BD5-4D03-82CC-3AC21542545D}'],
        'CMP_virtiofs_w11_amd64':['{643AE04C-905E-478D-A955-387DB8F65425}'],
        'CMP_WinServer2016_viogpudo':['{2749BFA1-F330-408C-9FB3-872D173DA3AE}'],
        'CMP_WinServer2019_viogpudo':['{F76D4E1B-9FF2-4B82-A5E3-17F64E6253FC}'],
        'CMP_WinServer2022_viogpudo':['{B541D990-26F9-4141-AC56-A39A804C3F4D}'],
        'CMP_WinServer2025_viogpudo':['{D135F903-7022-49E2-ABCE-497DF392381D}'],
        'CMP_Win10x64_viogpudo':['{0FEEC31F-F6E7-4865-AFD3-3E02451C46AD}'],
        'CMP_Win11x64_viogpudo':['{EAC432D2-C88F-49CA-90E7-7AC72974D325}'],
        'CMP_vgpusrv_2k16_amd64':['{614513BD-23B6-49C8-A780-D15F0EB09025}'],
        'CMP_viogpuap_2k16_amd64':['{173FB9D0-4A21-41BA-A70C-8AAD3D3A0FE1}'],
        'CMP_vgpusrv_2k19_amd64':['{87A3E13F-877D-4963-8AD4-6B9E09A0E47B}'],
        'CMP_viogpuap_2k19_amd64':['{DDF61C80-B14C-4C0A-9F1C-56E5EAB74705}'],
        'CMP_vgpusrv_2k22_amd64':['{62A74118-E380-4BD3-997B-4F3BDBE0F362}'],
        'CMP_viogpuap_2k22_amd64':['{B12866DF-4212-4E94-B3EC-57639F99F331}'],
        'CMP_vgpusrv_2k25_amd64':['{0DBC1A5D-F858-4EE2-AF3C-B4BBBAAEB91A}'],
        'CMP_viogpuap_2k25_amd64':['{148D86EF-A31B-4A27-848F-2BF8D61B113A}'],
        'CMP_vgpusrv_w10_amd64':['{100F48A8-BA46-4179-833F-6947ED755998}'],
        'CMP_viogpuap_w10_amd64':['{8951081D-CF31-40AD-B599-D003C1BA5ECB}'],
        'CMP_vgpusrv_w11_amd64':['{10B936E3-BA82-4FC4-B207-314D76B9D2ED}'],
        'CMP_viogpuap_w11_amd64':['{C897E664-AC4D-4BAC-BD6C-5BE6B18E9F3D}'],
        'CMP_WinServer2022_pvpanic':['{6623B93F-8C4A-4FD3-8A02-767B04682A42}'],
        'CMP_WinServer2025_pvpanic':['{EDAAE7D2-1E01-4870-B502-00BD679069C1}'],
        'CMP_Win11x64_pvpanic':['{B465A5A3-25A9-4924-9F6F-79BFE5066DC3}'],
        'CMP_WinServer2022_qemupciserial':['{9A944EAD-1E58-4AE0-A2FE-303FE4BCBA69}'],
#        'CMP_WinServer2025_qemupciserial':['{6ADE7045-CCA3-4DEE-BAB1-B42E06F8A01B}'],
        'CMP_Win11x64_qemupciserial':['{8D61E1C4-B9F4-4FDE-A171-5ECFB9AEADBA}'],
        'CMP_WinServer2022_vioinput':['{753A2041-BE3E-44F9-ABE3-394B7EBD2B46}'],
        'CMP_vioinput_viohidkmdf_2k22_amd64':['{D80F19F5-B422-4177-A9B9-6233C3A5EEDB}'],
        'CMP_WinServer2025_vioinput':['{ED2076F5-2462-4B9D-A796-008818C8DB38}'],
        'CMP_vioinput_viohidkmdf_2k25_amd64':['{AC486C71-64CF-4DC4-8D34-DD4BF602DC59}'],
        'CMP_Win11x64_vioinput':['{FDFC0048-91E6-4179-B235-0941C7EAEAA4}'],
        'CMP_vioinput_viohidkmdf_w11_amd64':['{283CA7B0-1797-410D-8B42-5255267C5DB0}'],
        'CMP_WinServer2016_fwcfg':['{7D32ABFB-E9A0-42D9-89F5-72E11B088989}'],
        'CMP_WinServer2019_fwcfg':['{B1C91741-7C0E-4041-A651-E6887B197A15}'],
        'CMP_WinServer2022_fwcfg':['{F20E93BC-E083-4E56-9AFD-4147369880AD}'],
        'CMP_WinServer2025_fwcfg':['{0A0FC2B9-A212-43A5-8E4B-5504104EC485}'],
        'CMP_Win10x64_fwcfg':['{73F380D6-BA74-44D3-82A3-40679FA73522}'],
        'CMP_Win11x64_fwcfg':['{7CD5A9FE-D145-4F82-BD09-EB4599C8558B}'],
        'CMP_WinServer2016_viomem':['{A3A319CF-70FF-4541-85D6-A2E5F64B29E4}'],
        'CMP_WinServer2019_viomem':['{BA2E41DE-297E-429B-A370-773CD61D0ECC}'],
        'CMP_WinServer2022_viomem':['{6B48367F-8EB2-4FB4-9CD6-1005C0049DF6}'],
        'CMP_WinServer2025_viomem':['{1C5C43F2-262C-4743-A515-8F9E2DED4DE4}'],
        'CMP_Win10x64_viomem':['{3F6F9752-C38E-4986-ACA6-2711FC6518D4}'],
        'CMP_Win11x64_viomem':['{F2D3A4F6-2ECA-4CC1-9260-7707B3A6034C}'],
    }

    expected_values_arm64 = {
        'CMP_Win10arm64_balloon':['{52DDE8BD-8E9F-4D64-8222-CE64CBACBB25}'],
        'CMP_Win11arm64_balloon':['{EC3F3C55-5330-4F16-9580-DD556F8EDAE9}'],
        'CMP_blnsvr_w10_ARM64':['{B27C38C9-4732-4C11-AD78-688B72D82A60}'],
        'CMP_blnsvr_w11_ARM64':['{3E431F47-41A5-439B-B812-0E62AD4EF9E6}'],
        'CMP_Win10arm64_netkvm':['{9287351C-4530-4E3A-9925-8DB1B3F59A29}'],
        'CMP_Win11arm64_netkvm':['{F7C567A5-3E01-4E37-8ED3-A63C610F4035}'],
        'CMP_netkvm_netkvmco_w10_ARM64':['{30DF0666-DF7A-4650-A86C-5A7F93C3830F}'],
        'CMP_netkvm_netkvmco_w11_ARM64':['{20ECCCF1-33CF-459E-AC6F-1A45380D84E9}'],
        'CMP_netkvm_netkvmp_w10_ARM64':['{52AE076A-B925-4554-AE83-CF900CC21D4A}'],
        'CMP_netkvm_netkvmp_w11_ARM64':['{2E18F43B-0E11-4B52-8385-4F10E7A0F0C2}'],
        'CMP_netkvm_readme_w10_ARM64':['{EDEE3B26-10E9-4653-8C4A-63C7DB974229}'],
        'CMP_netkvm_readme_w11_ARM64':['{C173058C-15D0-48CA-8661-71CE76BD6910}'],
        'CMP_Win10arm64_vioinput':['{7629C0DF-852D-477B-972D-A12D5F66820C}'],
        'CMP_Win11arm64_vioinput':['{4C942DEB-8A96-45B0-B698-FE6415AFA0FC}'],
        'CMP_vioinput_viohidkmdf_w10_ARM64':['{377221BD-6603-4ED1-B79B-F4686964FD6E}'],
        'CMP_vioinput_viohidkmdf_w11_ARM64':['{697E1271-3D7A-4D65-81C6-4A854FC1DD47}'],
        'CMP_Win10arm64_viorng':['{5E0C1423-CE6A-42BF-9C53-F733A3820B7F}'],
        'CMP_Win11arm64_viorng':['{1B05B7FD-7E74-472F-AD1B-EDFE09830288}'],
        'CMP_viorng_viorngum_w10_ARM64':['{BD33708E-ADB1-41B0-AD34-D847FE83385F}'],
        'CMP_viorng_viorngum_w11_ARM64':['{023A6BCC-E1BE-42F5-B5B7-DB646FAFFEBB}'],
        'CMP_Win10arm64_vioscsi':['{BE22C833-7A4E-4015-9EFF-36DF175A6936}'],
        'CMP_Win11arm64_vioscsi':['{E4A85170-E898-4735-A80C-7E2097638C59}'],
        'CMP_Win10arm64_vioser':['{2E905BF5-9EE5-4649-BE8A-41A5DD97B47A}'],
        'CMP_Win11arm64_vioser':['{8F7E00BD-7FD1-46D4-A9BC-CCF3584CCA5A}'],
        'CMP_Win10arm64_viostor':['{1DA289DE-9B38-494D-9DF4-48D381DD2B6A}'],
        'CMP_Win11arm64_viostor':['{50093013-592D-4D10-8084-EA3915F72BBF}'],
        'CMP_Win10arm64_viogpudo':['{7C80490D-B4D9-4A26-BAB3-E367224DCC69}'],
        'CMP_Win11arm64_viogpudo':['{BD8D94DC-9DA3-49D7-99BB-759C794D2BBA}'],
        'CMP_vgpusrv_w10_ARM64':['{02374F06-B727-478A-A945-76A75990EDC9}'],
        'CMP_vgpusrv_w11_ARM64':['{98D0EEE3-B4BE-4A72-8CC8-E4F568DE3120}'],
        'CMP_viogpuap_w10_ARM64':['{B5BD141C-5BC0-487B-B214-1069843DDF87}'],
        'CMP_viogpuap_w11_ARM64':['{4BC9AB4C-85C7-4453-B83B-E7836C8188D4}'],
        'CMP_Win10arm64_viomem':['{CDED3CC1-2AAE-42DA-AB03-7791A41967FC}'],
        'CMP_Win11arm64_viomem':['{4EDCD8A6-E304-4502-97F5-8A006E1A3E84}'],
    }

    expected_values_x86 = {
        'CMP_Win10x86_balloon':['{DF5DA075-CFCF-425A-A555-548E60B2835E}'],
        'CMP_blnsvr_w10_x86':['{908D56E1-A780-46CB-BAFC-8775E7BC2B7F}'],
        'CMP_Win10x86_netkvm':['{51E576FD-C649-4D6C-B30D-8FD5807318C1}'],
        'CMP_netkvm_netkvmp_w10_x86':['{52FD267B-0725-4B9B-A90F-601DC2B93F8C}'],
        'CMP_netkvm_netkvmco_w10_x86':['{06224827-B12C-451D-A0E7-1023747302E9}'],
        'CMP_netkvm_readme_w10_x86':['{D9A889B2-6D9A-45FF-AC88-7E0FFA4C44FA}'],
        'CMP_Win10x86_viorng':['{8702F843-FC88-4810-9BB4-E28560187290}'],
        'CMP_viorng_viorngum_w10_x86':['{675D7446-E963-4AF3-A4DB-FF3D749FDAF9}'],
        'CMP_Win10x86_vioscsi':['{3A763786-27EA-4967-86DB-D6D982D7CACB}'],
        'CMP_Win10x86_vioser':['{DF2A920C-E408-4479-8156-E86EC9DCA9D4}'],
        'CMP_Win10x86_viostor':['{E514AF40-771B-4028-96B3-0E0268A4C176}'],
        'CMP_Win10x86_viofs':['{19BCC8D3-F4E9-41C1-AF79-196C9F2B16B8}'],
        'CMP_virtiofs_w10_x86':['{CDC4821F-E00B-48BB-9A4A-5C1A420333F9}'],
        'CMP_Win10x86_viogpudo':['{D5AB2386-95A5-4ECD-A0CF-8F0DF40ABDD3}'],
        'CMP_vgpusrv_w10_x86':['{6B2A4D0D-68DD-4BD9-B776-D0B13F8E32D4}'],
        'CMP_viogpuap_w10_x86':['{5AD8C91E-1DAF-447A-A6F8-226883D7C233}'],
        'CMP_Win10x86_fwcfg':['{576398C6-231E-4FE0-8AF6-E62BF5015F70}'],
        }
    return (
        ("Component", "ComponentId"),
        {
            'x86': expected_values_x86,
            'x64': expected_values_x64,
            'arm64': expected_values_arm64
        }
    )


def get_expected_upgrade_code():
    expected_values_x64 = {'UpgradeCode':['{05294E2F-7778-4E46-B2A3-CF039D5767C8}']}
    expected_values_x86 = {'UpgradeCode':['{22C8F100-2ED1-4A99-A8FC-C391F0D1D7E7}']}
    expected_values_arm64 = {'UpgradeCode':['{10725300-348B-4908-B52E-4D71B15480F4}']}
    return (
        ("Property", "Value"),
        {
            'x86': expected_values_x86,
            'x64': expected_values_x64,
            'arm64': expected_values_arm64
        }
    )

def get_expected_allusers_property():
    expected_values_x64 = {'ALLUSERS':['1']}
    expected_values_x86 = {'ALLUSERS':['1']}
    expected_values_arm64 = {'ALLUSERS':['1']}
    return (
        ("Property", "Value"),
        {
            'x86': expected_values_x86,
            'x64': expected_values_x64,
            'arm64': expected_values_arm64
        }
    )

def get_expected_RegLocator():
    expected_values_x64 = {
        'ProductNameSearch': [r'SOFTWARE\Microsoft\Windows NT\CurrentVersion', 'ProductName'],
        'CurrentBuildNumberSearch': [r'SOFTWARE\Microsoft\Windows NT\CurrentVersion', 'CurrentBuildNumber'],
        'wgt_uninstall_path': [r'SOFTWARE\Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall\{042CD13C-735D-4140-B146-D3F8263D7E4C}', 'UninstallString']
    }
    expected_values_x86 = {
        'ProductNameSearch':[r'SOFTWARE\Microsoft\Windows NT\CurrentVersion', 'ProductName'],
        'wgt_uninstall_path':[r'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{042CD13C-735D-4140-B146-D3F8263D7E4C}', 'UninstallString'],
    }
    expected_values_arm64 = {
        'ProductNameSearch': [r'SOFTWARE\Microsoft\Windows NT\CurrentVersion', 'ProductName'],
        'CurrentBuildNumberSearch': [r'SOFTWARE\Microsoft\Windows NT\CurrentVersion', 'CurrentBuildNumber'],
        'ProcessorArchitectureSearch': [r'SYSTEM\CurrentControlSet\Control\Session Manager\Environment', 'PROCESSOR_ARCHITECTURE'],
        'wgt_uninstall_path': [r'SOFTWARE\Wow6432Node\Microsoft\Windows\CurrentVersion\Uninstall\{042CD13C-735D-4140-B146-D3F8263D7E4C}', 'UninstallString']
    }
    return (
        ("Signature", "Key", "Name"),
        {
            'x86': expected_values_x86,
            'x64': expected_values_x64,
            'arm64': expected_values_arm64
        }
    )

