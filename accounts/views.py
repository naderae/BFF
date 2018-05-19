from django.shortcuts import render, redirect, get_object_or_404
from django.contrib.auth.models import User
from django.contrib import auth
from groups.models import Group
from posts.models import Post
from friends.models import Friend
from accounts.models import File
from locations.models import Location
# from .forms import FileForm
from django.forms import modelformset_factory

# Create your views here.

def home(request):
    return render(request, 'accounts/home.html')

def signup(request):
    if request.method == 'POST':
        if request.POST['password1'] == request.POST['password2']:
            try:
                User.objects.get(username=request.POST['username'])
                return render(request, 'accounts/signup.html', {'error': "An account with this username already exists"})
            except User.DoesNotExist:
                user = User.objects.create_user(request.POST['username'], password=request.POST['password1'])
                auth.login(request, user)
                return redirect('groups:index')



        else:
            return render(request, 'accounts/signup.html', {'error':'The passwords do not match'})
    else:
        return render(request, 'accounts/signup.html')

def login(request):
    if request.method == 'POST':

            user = auth.authenticate(username=request.POST['username'], password=request.POST['password1'])

            if user is not None:
                auth.login(request, user)
                return redirect('groups:index')
            else:
                return render(request, 'accounts/login.html', {'error':"An account with this information does not exist"})

    else:
        return render(request, 'accounts/login.html')

def logout(request):
    if request.method == 'POST':
        auth.logout(request)
        return redirect('accounts:login')


def profile(request, user_id):
    user_profile = get_object_or_404(User, pk=user_id)
    current_user = request.user
    groups = user_profile.group_set.all()
    posts = Post.objects.filter(author=user_profile)
    pics = File.objects.filter(user=user_profile)[:6]
    all_users = User.objects.all()
    user_locations = Location.objects.filter(user=user_profile)
    # get grabs one instance, so you grab an instance of friend.
    # you locate the friends list by its owner, which is 'current_user' in this case
    try:
        friend = Friend.objects.get(owner=user_profile)
        friends = friend.users.all()
    except Friend.DoesNotExist:
        # friend = None
        friends = None
    return render(request, 'accounts/profile.html', {'user_profile':user_profile, 'groups':groups, 'posts':posts, 'friends':friends, 'pics':pics, 'all_users':all_users, 'current_user':current_user, 'locations':user_locations})




def upload_pics(request, user_id):

    user_profile = get_object_or_404(User, pk=user_id)

    if request.method == "POST":

        file_list = request.FILES.getlist('files')
        for afile in file_list:
            pic = File()
            pic.user = user_profile
            pic.image = afile
            pic.save()
            # return redirect(request, 'accounts:profile', user_id)
        return redirect("groups:index")

    else:
        render(request, 'accounts/pic_upload.html')
    return render(request, 'accounts/pic_upload.html')
