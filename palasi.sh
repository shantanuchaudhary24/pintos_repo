if [ $# -lt 2 ];
then
	echo "Usage: ./palasi.sh <kerberos-id> <base|threads|userprog|vm|filesys>";
	exit
fi

rm .uploadlog
ssh $1@palasi.cse.iitd.ernet.in 'mkdir -v ~/svn; cd ~/svn; mv -v --backup=t pintos-repo .bak/; exit;' | tee -a .uploadlog
scp -r -v ../pintos-repo $1@palasi.cse.iitd.ernet.in:~/svn/pintos-repo | tee -a .uploadlog
ssh $1@palasi.cse.iitd.ernet.in -t 'cd ~/svn/pintos-repo; echo "Enter the URL of your repository (e.g., https://svn.iitd.ernet.in/~cs1112345/pintos-repo):\n" ; /usr/local/bin/course-scripts/csl373-submit-pintos '$2'; exit;' | tee -a .uploadlog
